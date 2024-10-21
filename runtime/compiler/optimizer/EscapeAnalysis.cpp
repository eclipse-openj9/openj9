/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifdef J9ZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include "optimizer/EscapeAnalysis.hpp"
#include "optimizer/EscapeAnalysisTools.hpp"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "cs2/bitvectr.h"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/TypeLayout.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Checklist.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/String.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/LocalOpts.hpp"
#include "optimizer/MonitorElimination.hpp"
#include "ras/Debug.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9Runtime.hpp"

#define OPT_DETAILS "O^O ESCAPE ANALYSIS: "

#define MAX_SIZE_FOR_ONE_CONTIGUOUS_OBJECT   2416 // Increased from 72
#define MAX_SIZE_FOR_ALL_OBJECTS             3000 // Increased from 500
#define MAX_SNIFF_BYTECODE_SIZE              1600

#define LOCAL_OBJECTS_COLLECTABLE 1

static bool blockIsInLoop(TR::Block *block)
   {
   for (TR_Structure *s = block->getStructureOf()->getParent(); s; s = s->getParent())
      {
      TR_RegionStructure *region = s->asRegion();
      if (region->isNaturalLoop() || region->containsInternalCycles())
         return true;
      }
   return false;
   }

TR_EscapeAnalysis::TR_EscapeAnalysis(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _newObjectNoZeroInitSymRef(NULL),
     _newValueSymRef(NULL),
     _newArrayNoZeroInitSymRef(NULL),
     _dependentAllocations(manager->comp()->trMemory()),
     _inlineCallSites(manager->comp()->trMemory()),
     _dememoizedAllocs(manager->comp()->trMemory()),
     _devirtualizedCallSites(manager->comp()->trMemory()),
     _initializedHeapifiedTemps(NULL),
     _aNewArrayNoZeroInitSymRef(NULL)
   {
   static char *disableValueTypeEASupport = feGetEnv("TR_DisableValueTypeEA");
   _disableValueTypeStackAllocation = (disableValueTypeEASupport != NULL);

   _newObjectNoZeroInitSymRef = comp()->getSymRefTab()->findOrCreateNewObjectNoZeroInitSymbolRef(0);
   _newValueSymRef = comp()->getSymRefTab()->findOrCreateNewValueSymbolRef(0);
   _newArrayNoZeroInitSymRef  = comp()->getSymRefTab()->findOrCreateNewArrayNoZeroInitSymbolRef(0);
   _aNewArrayNoZeroInitSymRef = comp()->getSymRefTab()->findOrCreateANewArrayNoZeroInitSymbolRef(0);
   _maxPassNumber = 0;

   _dememoizationSymRef = NULL;

   _createStackAllocations   = true;
   _createLocalObjects       = cg()->supportsStackAllocations();
   _desynchronizeCalls       = true;
#if CHECK_MONITORS
   /* monitors */
   _removeMonitors           = true;
#endif
   }

char *TR_EscapeAnalysis::getClassName(TR::Node *classNode)
   {
   char *className = NULL;

   if (classNode->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference *symRef = classNode->getSymbolReference();

      if (symRef->getSymbol()->isClassObject())
         {
         int32_t  classNameLength;
         char    *classNameChars = TR::Compiler->cls.classNameChars(comp(), symRef,  classNameLength);

         if (NULL != classNameChars)
            {
            className = (char *)trMemory()->allocateStackMemory(classNameLength+1, TR_Memory::EscapeAnalysis);
            memcpy(className, classNameChars, classNameLength);
            className[classNameLength] = 0;
            }
         }
      }
   return className;
   }

bool TR_EscapeAnalysis::isImmutableObject(TR::Node *node)
   {
   // For debugging issues with the special handling of immutable objects
   // that allows them to be discontiguously allocated even if they escape
   static char *disableImmutableObjectHandling = feGetEnv("TR_disableEAImmutableObjectHandling");

   if (disableImmutableObjectHandling)
      {
      return false;
      }

   if (node->getOpCodeValue() == TR::newvalue)
      {
      return true;
      }

   if (node->getOpCodeValue() != TR::New)
      {
      return false;
      }

   TR::Node *classNode = node->getFirstChild();

   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *)classNode->getSymbol()->getStaticSymbol()->getStaticAddress();
   if (TR::Compiler->cls.isValueTypeClass(clazz))
      {
      return true;
      }

   char *className = getClassName(classNode);

   if (NULL != className &&
          !strncmp("java/lang/", className, 10) &&
             (!strcmp("Integer", &className[10]) ||
              !strcmp("Long", &className[10]) ||
              !strcmp("Short", &className[10]) ||
              !strcmp("Byte", &className[10]) ||
              !strcmp("Boolean", &className[10]) ||
              !strcmp("Character", &className[10]) ||
              !strcmp("Double", &className[10]) ||
              !strcmp("Float", &className[10])))
      {
      return true;
      }


   return false;
   }

bool TR_EscapeAnalysis::isImmutableObject(Candidate *candidate)
   {
   if (candidate->_isImmutable)
      return true;

   bool b = isImmutableObject(candidate->_node);
   candidate->_isImmutable = b;
   return b;
   }



int32_t TR_EscapeAnalysis::perform()
   {
   if (comp()->isOptServer() && (comp()->getMethodHotness() <= warm))
      return 0;

   // EA generates direct stores/loads to instance field which is different
   // from a normal instance field read/write. Field watch would need special handling
   // for stores/loads generated by EA.
   if (comp()->incompleteOptimizerSupportForReadWriteBarriers())
      return 0;

   static char *doESCNonQuiet = feGetEnv("TR_ESCAPENONQUIET");
   if (doESCNonQuiet && comp()->getOutFile() == NULL)
      return 0;

   int32_t nodeCount = 0;
   vcount_t visitCount = comp()->incVisitCount(); //@TODO: needs a change to TR_Node's
                                                  //countNumberOfNodesInSubtree
                                                  //we should probably leave it as is
                                                  //for the next iteration
   TR::TreeTop *tt = comp()->getStartTree();
   for (; tt; tt = tt->getNextTreeTop())
      nodeCount += tt->getNode()->countNumberOfNodesInSubtree(visitCount);

   // Set thresholds depending on the method's hotness
   //
   TR_Hotness methodHotness = comp()->getMethodHotness();
   if (methodHotness > hot)
      {
      _maxPassNumber          = 6;
      _maxSniffDepth          = 8;
      _maxInlinedBytecodeSize = 5000 - nodeCount;
      }
   else
      {
      _maxPassNumber          = 3;
      //_maxPassNumber          = (methodHotness < hot) ? 2 : 3;
      _maxSniffDepth          = 4;
      _maxInlinedBytecodeSize = 4000 - nodeCount;
      }

   // under HCR we can protect the top level sniff with an HCR guard
   // nested sniffs are not currently supported
   if (comp()->getHCRMode() != TR::none)
      _maxSniffDepth = 1;

   TR_ASSERT_FATAL(_maxSniffDepth < 16, "The argToCall and nonThisArgToCall flags are 16 bits - a depth limit greater than 16 will not fit in these flags");

   if (getLastRun())
      _maxPassNumber = 0; // Notwithstanding our heuristics, if this is the last run, our max "pass number" is zero (which is the first pass)

   _maxPeekedBytecodeSize  = comp()->getMaxPeekedBytecodeSize();

   // Escape analysis is organized so that it may decide another pass of
   // the analysis is required immediately after the current one. It leaves
   // it up to the optimization strategy to re-invoke the analysis, but we
   // keep track here of the number of passes through the analysis so we
   // can restrict the number of passes. Each time we end the local loop of
   // passes the current pass number is reset to zero in case escape analysis
   // is called again later in the optimization strategy.
   //
   if (manager()->numPassesCompleted() == 0)
      {
      //
      void *data = manager()->getOptData();
      TR_BitVector *peekableCalls = NULL;
      if (data != NULL)
         {
         peekableCalls = ((TR_EscapeAnalysis::PersistentData *)data)->_peekableCalls;
         delete ((TR_EscapeAnalysis::PersistentData *) data) ;
         manager()->setOptData(NULL);
         }
      manager()->setOptData(new (comp()->allocator()) TR_EscapeAnalysis::PersistentData(comp()));
      if (peekableCalls != NULL)
         ((TR_EscapeAnalysis::PersistentData *)manager()->getOptData())->_peekableCalls = peekableCalls;
      }
   else
      {
      if (trace())
         {
         /////printf("secs Performing pass %d of Escape Analysis for %s\n", _currentPass, comp()->signature());
         }
      }

   int32_t cost = 0;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   _callsToProtect = new (trStackMemory()) CallLoadMap(CallLoadMapComparator(), comp()->trMemory()->currentStackRegion());

#if CHECK_MONITORS
   /* monitors */
   TR_MonitorStructureChecker inspector;
   if (inspector.checkMonitorStructure(comp()->getFlowGraph()))
      {
      _removeMonitors = false;
      if (trace())
         traceMsg(comp(), "Disallowing monitor-removal because of strange monitor structure\n");
      }
#endif

   cost = performAnalysisOnce();

   if (!_callsToProtect->empty() && manager()->numPassesCompleted() < _maxPassNumber)
      {
      TR::CFG *cfg = comp()->getFlowGraph();
      TR::Block *block = NULL;
      TR::TreeTop *lastTree = comp()->findLastTree();
      TR_EscapeAnalysisTools tools(comp());
      RemainingUseCountMap *remainingUseCount = new (comp()->trStackMemory()) RemainingUseCountMap(RemainingUseCountMapComparator(), comp()->trMemory()->currentStackRegion());
      for (TR::TreeTop *tt = comp()->findLastTree(); tt != NULL; tt = tt->getPrevTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node->getOpCodeValue() == TR::BBEnd)
            block = node->getBlock();
         else if (node->getStoreNode() && node->getStoreNode()->getOpCode().isStoreDirect())
            node = node->getStoreNode()->getFirstChild();
         else if (node->getOpCode().isCheck() || node->getOpCode().isAnchor() || node->getOpCodeValue() == TR::treetop)
            node = node->getFirstChild();

         auto nodeLookup = _callsToProtect->find(node);
         if (nodeLookup == _callsToProtect->end()
             || TR_EscapeAnalysisTools::isFakeEscape(node)
             || node->getSymbol() == NULL
             || node->getSymbol()->getResolvedMethodSymbol() == NULL
             || node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod() == NULL)
            continue;

         if (node->getReferenceCount() > 1)
            {
            auto useCount = remainingUseCount->find(node);
            if (useCount == remainingUseCount->end())
               {
               (*remainingUseCount)[node] = node->getReferenceCount() - 1;
               continue;
               }
            if (useCount->second > 1)
               {
               useCount->second -= 1;
               continue;
               }
            }

         if (!performTransformation(comp(), "%sHCR CALL PEEKING: Protecting call [%p] n%dn with an HCR guard and escape helper\n", OPT_DETAILS, node, node->getGlobalIndex()))
            continue;

         ((TR_EscapeAnalysis::PersistentData*)manager()->getOptData())->_processedCalls->set(node->getGlobalIndex());

         _repeatAnalysis = true;

         optimizer()->setValueNumberInfo(NULL);
         cfg->invalidateStructure();

         TR::TreeTop *prevTT = tt->getPrevTreeTop();
         TR::Block *callBlock = block->split(tt, comp()->getFlowGraph(), true, true);

         // check uncommoned nodes for stores of the candidate - if so we need to add the new temp to the
         // list of loads
         for (TR::TreeTop *itr = block->getExit(); itr != prevTT; itr = itr->getPrevTreeTop())
            {
            TR::Node *storeNode = itr->getNode()->getStoreNode();
            if (storeNode
                && storeNode->getOpCodeValue() == TR::astore
                && nodeLookup->second.first->get(storeNode->getFirstChild()->getGlobalIndex()))
               {
               nodeLookup->second.second->set(storeNode->getSymbolReference()->getReferenceNumber());
               }
            }

         TR::Node *guard = TR_VirtualGuard::createHCRGuard(comp(),
                              node->getByteCodeInfo().getCallerIndex(),
                              node,
                              NULL,
                              node->getSymbol()->getResolvedMethodSymbol(),
                              node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->classOfMethod());
         block->getExit()->insertBefore(TR::TreeTop::create(comp(), guard));

         TR::Block *heapificationBlock = TR::Block::createEmptyBlock(node, comp(), MAX_COLD_BLOCK_COUNT);
         heapificationBlock->getExit()->join(lastTree->getNextTreeTop());
         lastTree->join(heapificationBlock->getEntry());
         lastTree = heapificationBlock->getExit();
         heapificationBlock->setIsCold();

         guard->setBranchDestination(heapificationBlock->getEntry());
         cfg->addNode(heapificationBlock);
         cfg->addEdge(block, heapificationBlock);
         cfg->addEdge(heapificationBlock, callBlock);

         heapificationBlock->getExit()->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(node, TR::Goto, 0, callBlock->getEntry())));
         tools.insertFakeEscapeForLoads(heapificationBlock, node, *nodeLookup->second.second);
         traceMsg(comp(), "Created heapification block_%d\n", heapificationBlock->getNumber());

         ((TR_EscapeAnalysis::PersistentData*)manager()->getOptData())->_peekableCalls->set(node->getGlobalIndex());
         _callsToProtect->erase(nodeLookup);
         tt = prevTT->getNextTreeTop();
         }
      }
   } // scope of the stack memory region

   if (_repeatAnalysis && manager()->numPassesCompleted() < _maxPassNumber)
      {
      // Ask the optimizer to repeat this analysis
      //
      requestOpt(OMR::eachEscapeAnalysisPassGroup);
      manager()->incNumPassesCompleted();
      }
   else
      {
      // Don't repeat this analysis, reset the pass count for next time
      //
      manager()->setNumPassesCompleted(0);
      }

   return cost;
   }

const char *
TR_EscapeAnalysis::optDetailString() const throw()
   {
   return "O^O ESCAPE ANALYSIS: ";
   }

void TR_EscapeAnalysis::rememoize(Candidate *candidate, bool mayDememoizeNextTime)
   {
   if (!candidate->_dememoizedConstructorCall)
      return;

   TR_ASSERT(candidate->_treeTop->getEnclosingBlock() == candidate->_dememoizedConstructorCall->getEnclosingBlock(),
      "Dememoized constructor call %p must be in the same block as allocation %p", candidate->_treeTop->getNode(), candidate->_dememoizedConstructorCall->getNode());
   if (trace())
      traceMsg(comp(), "   Rememoizing%s [%p] using constructor call [%p]\n", mayDememoizeNextTime?"":" and inlining", candidate->_node, candidate->_dememoizedConstructorCall->getNode()->getFirstChild());

   // Change trees back
   //
   candidate->_node->getFirstChild()->recursivelyDecReferenceCount(); // remove loadaddr of class
   candidate->_node->setAndIncChild(0, candidate->_dememoizedConstructorCall->getNode()->getFirstChild()->getSecondChild()); // original call argument
   TR::Node::recreate(candidate->_node, TR::acall);
   candidate->_node->setSymbolReference(candidate->_dememoizedMethodSymRef);
   candidate->_dememoizedConstructorCall->unlink(true);

   // Only rememoize once
   //
   _inlineCallSites.remove(candidate->_dememoizedConstructorCall);
   candidate->_dememoizedConstructorCall = NULL;
   candidate->_dememoizedMethodSymRef    = NULL;

   if (!mayDememoizeNextTime)
      {
      // Inline the memoization method so we can apply EA to it even though dememoization failed.
      // This also prevents us from re-trying dememoization when it's hopeless.
      //
      _inlineCallSites.add(candidate->_treeTop);
      }
   }

static const char *ynmString(TR_YesNoMaybe arg)
   {
   switch (arg)
      {
      case TR_yes:
         return "yes";
      case TR_no:
         return "no";
      case TR_maybe:
         return "maybe";
      }
   return "";
   }

static TR_YesNoMaybe ynmOr(TR_YesNoMaybe left, TR_YesNoMaybe right)
   {
   switch (left)
      {
      case TR_yes:
         return TR_yes;
      case TR_no:
         return right;
      case TR_maybe:
         return (right == TR_yes)? TR_yes : TR_maybe;
      }
   return TR_maybe;
   }

static TR_YesNoMaybe candidateHasField(Candidate *candidate, TR::Node *fieldNode, int32_t fieldOffset, TR_EscapeAnalysis *ea)
   {
   TR::Compilation *comp = ea->comp();
   TR::SymbolReference *fieldSymRef = fieldNode->getSymbolReference();
   int32_t fieldSize = fieldNode->getSize();

   int32_t minHeaderSize, maxHeaderSize;
   if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
      {
      minHeaderSize = maxHeaderSize = comp->fej9()->getObjectHeaderSizeInBytes();
      }
   else
      {
      minHeaderSize = std::min(TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), TR::Compiler->om.discontiguousArrayHeaderSizeInBytes());
      maxHeaderSize = std::max(TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), TR::Compiler->om.discontiguousArrayHeaderSizeInBytes());
      }

   TR_YesNoMaybe withinObjectBound       = TR_maybe;
   TR_YesNoMaybe withinObjectHeader      = TR_maybe;
   TR_YesNoMaybe belongsToAllocatedClass = TR_maybe;
   TR_YesNoMaybe result                  = TR_maybe;

   if (fieldOffset + fieldSize <= candidate->_size)
      withinObjectBound = TR_yes;
   else
      withinObjectBound = TR_no;

   if (fieldOffset + fieldSize <= minHeaderSize)
      withinObjectHeader = TR_yes;
   else if (fieldOffset > maxHeaderSize)
      withinObjectHeader = TR_no;
   else
      withinObjectHeader = TR_maybe;

   // Test a few conditions to try to avoid calling getDeclaringClassFromFieldOrStatic
   // because that requires VM access.
   //
   static char *debugEAFieldValidityCheck = feGetEnv("TR_debugEAFieldValidityCheck");
   if (withinObjectHeader == TR_yes)
      {
      result = TR_yes;
      }
   else
      {
      TR_OpaqueClassBlock *fieldClassInCP = fieldSymRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, fieldSymRef->getCPIndex());
      if (  fieldClassInCP
         && TR_yes == comp->fej9()->isInstanceOf((TR_OpaqueClassBlock*)candidate->_class, fieldClassInCP, true))
         {
         // Short cut: There's no need to look up the declaring class of the
         // field because we already know the candidate's class contains the
         // field since it inherits the class we're using to access the field
         // in the constant pool.
         //
         if (!debugEAFieldValidityCheck
            || performTransformation(comp, "%sQuick Using candidateHasField=yes (withinObjectBound=%s) for candidate [%p] field access [%p]\n",
               OPT_DETAILS, ynmString(withinObjectBound), candidate->_node, fieldNode))
            {
            belongsToAllocatedClass = TR_yes;
            result = TR_yes;
            }
         }
      }

   // If we're still not sure, go ahead and try getDeclaringClassFromFieldOrStatic
   //
   if (result == TR_maybe)
      {
      TR::VMAccessCriticalSection candidateHasFieldCriticalSection(comp->fej9(),
                                                                    TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                    comp);

      if (candidateHasFieldCriticalSection.hasVMAccess())
         {
         TR_OpaqueClassBlock *fieldDeclaringClass = fieldSymRef->getOwningMethod(comp)->getDeclaringClassFromFieldOrStatic(comp, fieldSymRef->getCPIndex());
         if (fieldDeclaringClass)
            belongsToAllocatedClass = comp->fej9()->isInstanceOf((TR_OpaqueClassBlock*)candidate->_class, fieldDeclaringClass, true);

         result = ynmOr(withinObjectHeader, belongsToAllocatedClass);

         if (debugEAFieldValidityCheck)
            {
            if (!performTransformation(comp, "%sUsing candidateHasField=%s (withinObjectBound=%s) for candidate [%p] field access [%p]\n",
                  OPT_DETAILS, ynmString(result), ynmString(withinObjectBound), candidate->_node, fieldNode))
               {
               // Conservatively return TR_no.
               // We do the performTransformation even if result is already TR_no
               // just to support countOptTransformations.
               //
               result = TR_no;
               }
            }
         }
      else if (ea->trace())
         {
         traceMsg(comp, "   Unable to acquire vm access; conservatively assume field [%p] does not belong to candidate [%p]\n", fieldNode, candidate->_node);
         }
      }

   if (debugEAFieldValidityCheck)
      {
      if (  (withinObjectBound != result)
         && !performTransformation(comp, "%sSubstituting candidateHasField=%s (withinObjectBound=%s) for candidate [%p] field access [%p]\n",
            OPT_DETAILS, ynmString(result), ynmString(withinObjectBound), candidate->_node, fieldNode))
         {
         // Use old logic for debugging purposes
         // Note: This is not necessarily correct, hence the env var guard.
         // Once we have confidence in the newer logic, this withinObjectBound
         // logic can eventually be deleted.
         //
         result = withinObjectBound;
         }
      }

   if (ea->trace())
      traceMsg(comp, "   Candidate [%p] field access [%p] candidateHasField=%s (withinObjectBound=%s withinObjectHeader=%s belongsToAllocatedClass=%s)\n",
         candidate->_node, fieldNode,
         ynmString(result),
         ynmString(withinObjectBound),
         ynmString(withinObjectHeader),
         ynmString(belongsToAllocatedClass));

   return result;
   }

//
// Hack markers
//

// PR 78801: Currently, BCD shadows don't have reliable size information, and
// BCD temps require size information.  This makes it hard to create BCD temps
// from shadows.
//
#define CANT_CREATE_BCD_TEMPS (1)

int32_t TR_EscapeAnalysis::performAnalysisOnce()
   {
   int32_t    cost = 0;
   Candidate *candidate, *next;

   if (trace())
      {
      traceMsg(comp(), "Starting Escape Analysis pass %d\n", manager()->numPassesCompleted());
      comp()->dumpMethodTrees("Trees before Escape Analysis");
      }

   _useDefInfo                 = NULL; // Build these only if required
   _invalidateUseDefInfo       = false;
   _valueNumberInfo            = NULL;
   _otherDefsForLoopAllocation = NULL;
   _methodSymbol               = NULL;
   _nodeUsesThroughAselect     = NULL;
   _repeatAnalysis             = false;
   _somethingChanged           = false;
   _inBigDecimalAdd            = false;
   _candidates.setFirst(NULL);
   _inlineCallSites.deleteAll();
   _dependentAllocations.deleteAll();
   _fixedVirtualCallSites.setFirst(NULL);

   _parms = NULL;
   _ignorableUses = NULL;
   _nonColdLocalObjectsValueNumbers = NULL;
   _allLocalObjectsValueNumbers = NULL;
   _visitedNodes = NULL;
   _aliasesOfAllocNode = NULL;
   _aliasesOfOtherAllocNode = NULL;
   _notOptimizableLocalObjectsValueNumbers = NULL;
   _notOptimizableLocalStringObjectsValueNumbers = NULL;
   _initializedHeapifiedTemps = NULL;

   // Walk the trees and find the "new" nodes.
   // Any that are candidates for local allocation or desynchronization are
   // added to the list of candidates.
   //
   findCandidates();
   cost++;

   if (!_candidates.isEmpty())
      {
      _useDefInfo = optimizer()->getUseDefInfo();
      _blocksWithFlushOnEntry = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
      _visitedNodes = new (trStackMemory()) TR_BitVector(comp()->getNodeCount(), trMemory(), stackAlloc, growable);
      _aliasesOfAllocNode = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
      _aliasesOfOtherAllocNode = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);

      if (!_useDefInfo)
         {
         if (trace())
            traceMsg(comp(), "Can't do Escape Analysis, no use/def information\n");
         _candidates.setFirst(NULL);
         }

      _valueNumberInfo = optimizer()->getValueNumberInfo();
      if (!_valueNumberInfo)
         {
         if (trace())
            traceMsg(comp(), "Can't do Escape Analysis, no value number information\n");
         _candidates.setFirst(NULL);
         }
      else
         {
         _ignorableUses = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
         _nonColdLocalObjectsValueNumbers = new (trStackMemory()) TR_BitVector(_valueNumberInfo->getNumberOfValues(), trMemory(), stackAlloc);
         _allLocalObjectsValueNumbers = new (trStackMemory()) TR_BitVector(_valueNumberInfo->getNumberOfValues(), trMemory(), stackAlloc);
         _notOptimizableLocalObjectsValueNumbers = new (trStackMemory()) TR_BitVector(_valueNumberInfo->getNumberOfValues(), trMemory(), stackAlloc);
          _notOptimizableLocalStringObjectsValueNumbers = new (trStackMemory()) TR_BitVector(_valueNumberInfo->getNumberOfValues(), trMemory(), stackAlloc);
         }
      }

   if ( !_candidates.isEmpty())
      {
      findLocalObjectsValueNumbers();
      findIgnorableUses();
      }

   // Complete the candidate info by finding all uses and defs that are reached
   // from each candidate.
   //
   if (!_candidates.isEmpty())
      {
      checkDefsAndUses();
      cost++;
      }

   if (trace())
      printCandidates("Initial candidates");

   // Look through the trees to see which candidates escape the method. This
   // may involve sniffing into called methods.
   //
   _sniffDepth = 0;
   _parms      = NULL;
   if (!_candidates.isEmpty())
      {
      //if (comp()->getMethodSymbol()->mayContainMonitors())
      if (cg()->getEnforceStoreOrder())
         {
         TR_FlowSensitiveEscapeAnalysis flowSensitiveAnalysis(comp(), optimizer(), comp()->getFlowGraph()->getStructure(), this);
         }

      bool ignoreRecursion = false;
      checkEscape(comp()->getStartTree(), false, ignoreRecursion);
      cost++;
      }

   //fixup those who have virtual calls
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();

      if (!candidate->isLocalAllocation())
         continue;

      if (!candidate->hasVirtualCallsToBeFixed())
         continue;

      dumpOptDetails(comp(), "Fixup indirect calls sniffed for candidate =%p\n",candidate->_node);
      TR::TreeTop *callSite, *callSiteNext = NULL;

      ListIterator<TR::TreeTop> it(candidate->getVirtualCallSitesToBeFixed());
      for (callSite = it.getFirst(); callSite; callSite = callSiteNext)
         {
         //TR::TreeTop *directCallTree = findCallSiteFixed(callSite);
         bool found = findCallSiteFixed(callSite);
         callSiteNext = it.getNext();
         if (!found)
            {
            if (!_devirtualizedCallSites.find(callSite))
               _devirtualizedCallSites.add(callSite);

            _fixedVirtualCallSites.add(new (trStackMemory()) TR_EscapeAnalysis::TR_CallSitesFixedMapper(callSite, NULL));
            dumpOptDetails(comp(), "adding Map:vCall = %p direct = %p\n",callSite, NULL);
            _repeatAnalysis = true;
            _somethingChanged = true;

            candidate->getCallSites()->remove(callSite);
            //candidate->getCallSites()->add(directCallTree);
            }
         else
            {
            dumpOptDetails(comp(), "found MAp for %p\n",callSite);
            //fixup the callSite list
            candidate->getCallSites()->remove(callSite);
            //candidate->getCallSites()->add(directCallTree);
            }
         }

      candidate->setLocalAllocation(false);
      if (trace())
         traceMsg(comp(), "   Make [%p] non-local because we'll try it again in another pass\n", candidate->_node);

      }

   //fixup those callSite which were devirtualize
   //fixup the coldBlockInfo too
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if (!candidate->isLocalAllocation())
         continue;

      TR::TreeTop *callSite, *callSiteNext = NULL;

      ListIterator<TR::TreeTop> it(candidate->getCallSites());
      for (callSite = it.getFirst(); callSite; callSite = callSiteNext)
         {
         callSiteNext = it.getNext();
         //TR::TreeTop *directCallTree = findCallSiteFixed(callSite);
         bool found = findCallSiteFixed(callSite);
         if (found)
            {
            if (trace())
               traceMsg(comp(), "replacing callsite %p for candidate %p with it's direct call\n",callSite,candidate->_node);
            candidate->getCallSites()->remove(callSite);
            candidate->setLocalAllocation(false);
            //candidate->getCallSites()->add(directCallTree);
            }
         }

      ListIterator<TR_ColdBlockEscapeInfo> coldBlockInfoIt(candidate->getColdBlockEscapeInfo());
      for (TR_ColdBlockEscapeInfo *info = coldBlockInfoIt.getFirst(); info != NULL; info = coldBlockInfoIt.getNext())
         {
         ListIterator<TR::TreeTop> treesIt(info->getTrees());
         for (TR::TreeTop *escapeTree = treesIt.getFirst(); escapeTree != NULL; escapeTree = treesIt.getNext())
            {
            if (findCallSiteFixed(escapeTree))
               {
               candidate->setLocalAllocation(false);
               break;
               }
            }
         if (!candidate->isLocalAllocation())
            break;
         }
      }

   // Check whether tentative dememoization can proceed
   //
   bool shouldRepeatDueToDememoization = false;
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();

      if (candidate->_dememoizedConstructorCall)
         {
         if (  candidate->isLocalAllocation()
            && !candidate->mustBeContiguousAllocation()
            && candidate->getCallSites()->isSingleton()
            && candidate->getCallSites()->find(candidate->_dememoizedConstructorCall)
            && performTransformation(comp(), "%sDememoizing [%p]\n", OPT_DETAILS, candidate->_node))
            {
            // Dememoization worked!
            // Inline the constructor and catch this candidate on the next pass
            candidate->setLocalAllocation(false);

            if (trace())
                 traceMsg(comp(), "2 setting local alloc %p to false\n", candidate->_node);

            _inlineCallSites.add(candidate->_dememoizedConstructorCall);
            _repeatAnalysis = true;
            }
         else
            {
            // Dememoization failed on this pass; must re-memoize instead

            bool mayDememoizeNextTime = true;

            // allow the call to be inlined at least in the last pass
            // and prevent from dememoizing the call again
            //
            if (candidate->isLocalAllocation())
               _repeatAnalysis = true;

            if (_repeatAnalysis)
               {
               if (manager()->numPassesCompleted() == _maxPassNumber-1)
                  mayDememoizeNextTime = false;
               else
                  shouldRepeatDueToDememoization = true;
               }
            else
               mayDememoizeNextTime = false;

            if (trace())
               {
               traceMsg(comp(), "   Fail [%p] because dememoization failed; will%s attempt again on next EA pass\n", candidate->_node, mayDememoizeNextTime? "":" NOT");
               traceMsg(comp(), "   Fail [%p] because dememoization failed; will%s attempt dememoization again on next EA pass\n", candidate->_node, mayDememoizeNextTime? "":" NOT");
               traceMsg(comp(), " 4 booleans are %d %d %d %d\n", candidate->isLocalAllocation(), candidate->mustBeContiguousAllocation(), candidate->getCallSites()->isSingleton(), candidate->getCallSites()->find(candidate->_dememoizedConstructorCall));
               }

            // if mayDememoizeNextTime is false, then the following call to
            // rememoize will add valueOf to the list of calls to be inlined
            //
            rememoize(candidate, mayDememoizeNextTime);
            if (trace())
               traceMsg(comp(), "8 removing cand %p to false\n", candidate->_node);
            }
         _candidates.remove(candidate);
         }
      }

   // Decide whether or not another pass of escape analysis is appropriate.
   // If we are allowed to do another pass and there are candidates which are
   // contiguous only because they are arguments to calls and those calls can
   // be inlined, then inline the calls and mark the candidates so that they
   // are not replaced in this pass.
   // This should result in the candidates being found to be non-contiguous
   // candidates in a later pass.
   //
   if (manager()->numPassesCompleted() < _maxPassNumber)
      {
      for (candidate = _candidates.getFirst(); candidate; candidate = next)
         {
         next = candidate->getNext();
         if (!candidate->isLocalAllocation())
            continue;

         if (trace())
             traceMsg(comp(), "   0 Look at [%p] must be %d\n", candidate->_node, candidate->mustBeContiguousAllocation());

         if (candidate->mustBeContiguousAllocation() || !candidate->hasCallSites() ||
             (candidate->_stringCopyNode && (candidate->_stringCopyNode != candidate->_node) &&
              !candidate->escapesInColdBlocks() &&
              !candidate->isLockedObject() &&
              !candidate->_seenSelfStore &&
              !candidate->_seenStoreToLocalObject))
            continue;


         if (trace())
            traceMsg(comp(), "   0.5 Look at [%p]\n", candidate->_node);

         // If any of the call sites for this parm is a guarded virtual call - there would be nothing
         // gained by inlining any of them - we will still end up with a call and will have to make
         // it contiguous
         //
         TR::TreeTop *callSite;
         bool seenGuardedCall = false;
         ListIterator<TR::TreeTop> it(candidate->getCallSites());
         for (callSite = it.getFirst(); callSite; callSite = it.getNext())
            {
            TR::Node *callNode = callSite->getNode();
            if (callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               seenGuardedCall = true;
               break;
               }
            }

         if (trace())
             traceMsg(comp(), "   1 Look at [%p]\n", candidate->_node);

         // If any of the calls is a guarded virtual call or
         // If the depth of inlining required is greater than the number of
         // passes left, or if the number of bytecodes needed to be inlined
         // will exceed the maximum, enough inlining can't be done.
         // In this case force the candidate to be contiguous.
         //
         if (seenGuardedCall ||
             candidate->_maxInlineDepth >= (_maxPassNumber-manager()->numPassesCompleted()) ||
             candidate->_inlineBytecodeSize > (_maxInlinedBytecodeSize-getOptData()->_totalInlinedBytecodeSize))
            {
            candidate->setMustBeContiguousAllocation();
            if (trace())
               traceMsg(comp(), "   Make [%p] contiguous because we can't inline enough\n", candidate->_node);
            continue;
            }

         // Take each call site that needs to be inlined and add it to the
         // list of inlined call sites
         //
         while ((callSite = candidate->getCallSites()->popHead()))
            {
            TR::Node *node = callSite->getNode();
            if (node->getOpCode().isTreeTop())
               node = node->getFirstChild();

            //traceMsg(comp(), "For alloc node %p call site %p\n", candidate->_node, node);
            //if (node->isTheVirtualCallNodeForAGuardedInlinedCall())
            if (!_inlineCallSites.find(callSite))
               {
               if (comp()->getMethodHotness() <= warm)
                  {
                  // Up to warm, inlining at this point can interfere with existing
                  // profiling and compilation heuristics, causing us to miss even
                  // better inlining opportunities in subsequent hot and scorching
                  // compiles.  Only inline selected methods where the benefit of
                  // profiling heuristics is outweighed by the cost of not inlining
                  // at warm.
                  //
                  // TODO: There must be a similar heuristic in the inliner for this
                  // sort of thing?  They ought to share code.
                  //
                  switch (node->getSymbol()->castToMethodSymbol()->getRecognizedMethod())
                     {
                     case TR::java_lang_Object_init:
                        break;
                     default:
                        candidate->setMustBeContiguousAllocation();
                        if (trace())
                           traceMsg(comp(), "   Make [%p] contiguous because we can't inline it at %s\n", candidate->_node, comp()->getHotnessName());
                        continue;
                     }
                  }

               _inlineCallSites.add(callSite);
               }
            }

         _repeatAnalysis = true;
         candidate->setLocalAllocation(false);
         if (trace())
            traceMsg(comp(), "   Make [%p] non-local because we'll try it again in another pass\n", candidate->_node);
         }
      }

   // Apply filters to candidates
   //
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();

      if (candidate->_kind == TR::New || candidate->_kind == TR::newvalue)
         {
         static bool doEAOpt = feGetEnv("TR_DisableEAOpt") ? false : true;
         if (!doEAOpt &&
             comp()->useCompressedPointers())
            {
            if (candidate->_seenSelfStore || candidate->_seenStoreToLocalObject)
               {
               candidate->setLocalAllocation(false);
               if (trace())
                  traceMsg(comp(), "   Make [%p] non-local because self store seen in compressed pointers mode\n", candidate->_node);
               }
            }

         if (  CANT_CREATE_BCD_TEMPS
            && candidate->isLocalAllocation()
            && !candidate->isContiguousAllocation()
            && candidate->_fields)
            {
            for (int32_t i = candidate->_fields->size()-1; i >= 0; i--)
               {
               TR::SymbolReference *field = candidate->_fields->element(i).fieldSymRef();
               if (field && field->getSymbol()->getDataType().isBCD())
                  {
                  candidate->setMustBeContiguousAllocation();
                  if (trace())
                     traceMsg(comp(), "   Make [%p] contiguous because we can't create temp for BCD field #%d\n", candidate->_node, field->getReferenceNumber());
                  break;
                  }
               }
            }

         if (candidate->isLocalAllocation() && candidate->isContiguousAllocation())
            {
            if (!_createLocalObjects)
               {
               candidate->setLocalAllocation(false);
               if (trace())
                  traceMsg(comp(), "   Make [%p] non-local because we can't create local objects\n", candidate->_node);
               }
            }


         if (candidate->isLocalAllocation() &&
             candidate->escapesInColdBlocks() &&
             (candidate->isLockedObject() ||
              candidate->_seenSelfStore ||
              candidate->_seenStoreToLocalObject))
            {
            candidate->setLocalAllocation(false);
            if (trace())
               traceMsg(comp(), "   Make [%p] non-local because we can't have locking when candidate escapes in cold blocks\n", candidate->_node);
            }

         // Primitive value type fields of objects created with a NEW bytecode must be initialized
         // with their default values.  EA is not yet set up to perform such iniitialization
         // if the value type's own fields have not been inlined into the class that
         // has a field of that type, so remove the candidate from consideration.
         if (candidate->_kind == TR::New)
            {
            TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *)candidate->_node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress();

            if (!TR::Compiler->cls.isZeroInitializable(clazz))
               {
               if (trace())
                  traceMsg(comp(), "   Fail [%p] because the candidate is not zero initializable (that is, it has a field of a primitive value type whose fields have not been inlined into this candidate's class)\n", candidate->_node);
               rememoize(candidate);
               _candidates.remove(candidate);
               continue;
               }
            }

         // If a contiguous candidate has reference slots, then stack-allocating it means putting
         // stores in the first block of the method.  If the first block is really hot, those stores
         // are expensive, and stack-allocation is probably not worthwhile.
         //
         bool objectHasReferenceFields = false;
#if LOCAL_OBJECTS_COLLECTABLE
         switch (candidate->_kind)
            {
            case TR::New:
            case TR::newvalue:
               if (comp()->fej9()->getReferenceSlotsInClass(comp(), (TR_OpaqueClassBlock *)candidate->_node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress()))
                  {
                  objectHasReferenceFields = true;
                  }
               break;
            case TR::anewarray:
               objectHasReferenceFields = true;
               break;
            default:
               break;
            }
#endif
         if (candidate->isLocalAllocation() &&
             (candidate->isInAColdBlock() ||
               (  objectHasReferenceFields
               && candidate->isContiguousAllocation()
               && (comp()->getStartBlock()->getFrequency() > 4*candidate->_block->getFrequency())))
             && !candidate->usedInNonColdBlock())
            //((candidate->isContiguousAllocation() && !candidate->lockedInNonColdBlock()) ||
            //  (!candidate->isContiguousAllocation() && !candidate->usedInNonColdBlock())))
            {
            candidate->setLocalAllocation(false);
             if (trace())
               traceMsg(comp(), "   Make [%p] non-local because the uses are not in hot enough blocks\n", candidate->_node);
            }

         // If the candidate has more than one value number, and a suspicious
         // field, reject the candidate to preserve r13 behaviour.  For
         // candidates with one value number, we can reach definite conclusions
         // about what to do with them because there's only one possibility.
         // (They're usually non-contiguous.)
         //
         if (candidate->_valueNumbers->size()> 1 && candidate->_fields)
            {
            for (int32_t i = candidate->_fields->size()-1; i >= 0; i--)
               {
               if (candidate->_fields->element(i).hasBadFieldSymRef())
                  {
                  if (trace())
                     traceMsg(comp(), "   Fail [%p] because candidate is dereferenced via a field that does not belong to allocated class\n", candidate->_node);
                  rememoize(candidate);
                  _candidates.remove(candidate);
                  break;
                  }
               }
            }

         if (candidate->_stringCopyNode && (candidate->_stringCopyNode != candidate->_node))
            {
            if (!candidate->escapesInColdBlocks() &&
                !candidate->isLockedObject() &&
                !candidate->_seenSelfStore &&
                !candidate->_seenStoreToLocalObject)
               candidate->setMustBeContiguousAllocation();
            else
               candidate->_stringCopyNode = NULL;
            }

         if (candidate->_dememoizedMethodSymRef && candidate->isContiguousAllocation())
            {
            if (trace())
               traceMsg(comp(), "   Fail [%p] because dememoized allocations must be non-contiguous\n", candidate->_node);
            rememoize(candidate);
            _candidates.remove(candidate);
            }

         continue;
         }

      // TODO-VALUETYPE: Need to handle allocation of null-restricted arrays if
      // null-restricted arrays are ever allocated using TR::anewarray and we
      // want to allow for stack allocation of null-restricted arrays
      if (candidate->_kind == TR::anewarray)
         {
         // Array Candidates for contiguous allocation that have unresolved
         // base classes must be rejected, since we cannot initialize the array
         // header.
         //
         if (candidate->isContiguousAllocation())
            {
            TR::Node *classNode = candidate->_node->getSecondChild();
            if (classNode->getOpCodeValue() != TR::loadaddr ||
                classNode->getSymbolReference()->isUnresolved())
               {
               if (trace())
                  traceMsg(comp(), "   Fail [%p] because base class is unresolved\n", candidate->_node);
               rememoize(candidate);
               _candidates.remove(candidate);
               }
            }
         }

      if (candidate->isProfileOnly() && candidate->isLocalAllocation())
         {
         candidate->setLocalAllocation(false);

         if (trace())
            traceMsg(comp(), "3 setting local alloc %p to false\n", candidate->_node);

         if (performTransformation(comp(), "%sInitiate value profiling for length of %s [%p]\n",OPT_DETAILS,
                                     candidate->_node->getOpCode().getName(),
                                     candidate->_node))
            {
            if (optimizer()->switchToProfiling())
               {
               TR::Recompilation *recomp          = comp()->getRecompilationInfo();
               TR_ValueProfiler *valueProfiler   = recomp? recomp->getValueProfiler() : NULL;
               TR::Node          *numElementsNode = candidate->_node->getFirstChild();

               TR_ByteCodeInfo originalBcInfo = TR_ProfiledNodeVersioning::temporarilySetProfilingBcInfoOnNewArrayLengthChild(candidate->_node, comp());
               valueProfiler->addProfilingTrees(numElementsNode, candidate->_treeTop, 5);
               numElementsNode->setByteCodeInfo(originalBcInfo);
               }
            else if (trace())
               {
               traceMsg(comp(), "  Unable to switch to profiling mode; no profiling trees added for [%p]\n", candidate->_node);
               }
            }
         }

      // Remove all array candidates that are not locally allocatable
      //
      if (!candidate->isLocalAllocation())
         {
         if (trace())
            traceMsg(comp(), "   Fail [%p] array candidate is not locally allocatable\n", candidate->_node);
         rememoize(candidate);
         _candidates.remove(candidate);
         continue;
         }
      }

   // Suppress stack allocation for any candidates that match the regular
   // expression specified with the suppressEA option.
   TR::SimpleRegex * suppressAtRegex = comp()->getOptions()->getSuppressEARegex();
   if (suppressAtRegex != NULL && !_candidates.isEmpty())
      {
      for (candidate = _candidates.getFirst(); candidate; candidate = next)
         {
         next = candidate->getNext();

         if (!candidate->isLocalAllocation())
            {
            continue;
            }

         TR_ByteCodeInfo &bcInfo = candidate->_node->getByteCodeInfo();
         if (TR::SimpleRegex::match(suppressAtRegex, bcInfo))
            {
            candidate->setLocalAllocation(false);

            if (trace())
               {
               traceMsg(comp(), "  Suppressing stack allocation of candidate node [%p] - matched suppressEA option\n", candidate->_node);
               }
            }
         }
      }

   // Check for size limits on total object allocation size.
   //
   if (!_candidates.isEmpty())
      {
      checkObjectSizes();
      cost++;
      }

   if (trace())
      printCandidates("Final candidates");

   // When we do stack allocation we generate number of stores
   // in the first block of the method, so that we can initialize the
   // stack allocated object correctly. However, if the first block ends up
   // being in a loop, those stores end up resetting the stack allocated object
   // even though the new might be under an if later on.
   if (comp()->getStartBlock() && !_candidates.isEmpty())
      {
      TR::Block *block = comp()->getStartBlock();

      if (blockIsInLoop(block))
         {
         comp()->getFlowGraph()->setStructure(NULL);
         TR::Block *firstBlockReplacement = toBlock(comp()->getFlowGraph()->addNode(
         TR::Block::createEmptyBlock(block->getEntry()->getNode(), comp(), std::max(MAX_COLD_BLOCK_COUNT+1, block->getFrequency()/10))));
         firstBlockReplacement->getExit()->join(block->getEntry());
         comp()->setStartTree(firstBlockReplacement->getEntry());

         TR::CFGEdge *edgeToRemove = comp()->getFlowGraph()->getStart()->asBlock()->getSuccessors().front();

         comp()->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(comp()->getFlowGraph()->getStart()->asBlock(),  firstBlockReplacement, trMemory()));
         comp()->getFlowGraph()->addEdge(TR::CFGEdge::createEdge(firstBlockReplacement,  block, trMemory()));

         comp()->getFlowGraph()->removeEdge(edgeToRemove);
         }
      }

   // Now each remaining candidate is known not to escape the method.
   // All synchronizations on the objects can be removed, and those
   // marked for local allocation can be locally allocated.
   // Go through the trees one more time, fixing up loads and stores for
   // the object.
   //
   if (!_candidates.isEmpty())
      {
      fixupTrees();
      cost++;
      }

   int32_t nonContiguousAllocations = 0;
   int32_t tempsCreatedForColdEscapePoints = 0;

   // Now fix up the new nodes themselves and insert any initialization code
   // that is necessary.
   //
   for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      if (candidate->isLocalAllocation())
         {
         if (performTransformation(comp(), "%sStack allocating candidate [%p]\n",OPT_DETAILS, candidate->_node))
            {
            //printf("stack allocation in %s %s\n",comp()->signature(),comp()->getHotnessName(comp()->getMethodHotness()));fflush(stdout);

            if (candidate->isContiguousAllocation())
               {
               if (candidate->_stringCopyNode && (candidate->_stringCopyNode != candidate->_node))
                  avoidStringCopyAllocation(candidate);
               else
                  makeContiguousLocalAllocation(candidate);
               }
            else
               {
               makeNonContiguousLocalAllocation(candidate);
               ++nonContiguousAllocations;
               }

            if (candidate->escapesInColdBlocks())
               {
               // _initializedHeapifiedTemps will contain any autos that are initialized in the entry block of
               // the method.  When the first candidate that escapes in a cold block is encountered, sweep
               // through the entry block looking for autos that are null initialized up until any potential
               // GC point or point at which an exception might be thrown, and record their sym refs in
               // _initializedHeapifiedTemps.  Then any autos that are associated with candidates that need to
               // be heapified will be explicitly null initialized in the entry block by heapifyForColdBlocks
               // if they have not already been found to be initialized there (i.e., not already listed in
               // _initializedHeapifiedTemps).  heapifyForColdBlocks will also add a new entry to
               // _initializedHeapifiedTemps for each sym ref it is obliged to explicitly null initialize.
               //
               if (_initializedHeapifiedTemps == NULL)
                  {
                  _initializedHeapifiedTemps = new(trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);

                  TR::Block *entryBlock = comp()->getStartBlock();
                  const bool entryHasExceptionSuccessors = entryBlock->hasExceptionSuccessors();

                  // Anything that might hold a reference to a potentially heapified object must be initialized
                  // on method entry.  Keep track of what is already null initialized in the first block on
                  // method entry to avoid redundant initializations that might occur in a large method that
                  // has many points at which heapification might occur.
                  //
                  for (TR::TreeTop *tt = comp()->getStartTree();
                       tt != NULL && tt->getNode()->getOpCodeValue() != TR::BBEnd;
                       tt = tt->getNextTreeTop())
                     {
                     TR::Node *node = tt->getNode();

                     // If the entry block has exception successors, any null initializations that follow
                     // a node that might raise an exception is not guaranteed to be executed, so stop
                     // recording entries in _initializedHeapifiedTemps.  Similarly, we need to ensure that
                     // all local initializations of autos that might hold a reference to a potentially
                     // heapified object are initialized before any GC might happen, so stop considering
                     // local initializations that follow a potential GC point.
                     //
                     if (node->canCauseGC() || entryHasExceptionSuccessors && node->exceptionsRaised())
                        {
                        break;
                        }

                     if (node->getOpCode().isStoreDirect()
                         && (node->getFirstChild()->getOpCodeValue() == TR::aconst)
                         && (node->getFirstChild()->getConstValue() == 0))
                        {
                        TR::SymbolReference *storeSymRef = node->getSymbolReference();

                        if (storeSymRef->getSymbol()->isAuto()
                            && !_initializedHeapifiedTemps->get(storeSymRef->getReferenceNumber()))
                           {
                           _initializedHeapifiedTemps->set(storeSymRef->getReferenceNumber());
                           }
                        }
                     }
                  }

               heapifyForColdBlocks(candidate);
               if (candidate->_fields)
                  {
                  int32_t i;
                  for (i = candidate->_fields->size()-1; i >= 0; i--)
                     {
                     if (((*candidate->_fields)[i]._symRef == NULL) &&
                         !(*candidate->_fields)[i].hasBadFieldSymRef())
                        {
                        //printf("Conservative aliasing reqd in %s\n", comp()->signature()); fflush(stdout);
                        comp()->getSymRefTab()->aliasBuilder.setConservativeGenericIntShadowAliasing(true);
                        }
                     }
                  }
               else
                  comp()->getSymRefTab()->aliasBuilder.setConservativeGenericIntShadowAliasing(true);

               tempsCreatedForColdEscapePoints++;
               }

            if (candidate->_seenFieldStore)
               _repeatAnalysis = true;


            _somethingChanged = true;
            }
         }
      }

   _somethingChanged |= devirtualizeCallSites();

   // If there are any call sites to be inlined, do it now
   //
   _somethingChanged |= inlineCallSites();

   // Use/def and value number information must be recalculated for later
   // optimization passes.
   //
   if (_somethingChanged || _invalidateUseDefInfo)
      {
      optimizer()->setUseDefInfo(NULL);
      _useDefInfo = NULL;
      }
   if (_somethingChanged)
      {
      optimizer()->setValueNumberInfo(NULL);
      requestOpt(OMR::treeSimplification);
      requestOpt(OMR::globalValuePropagation);
      }
   else
      {
      if (!shouldRepeatDueToDememoization)
         _repeatAnalysis = false;
      else
         {
         // fast forward the current pass to the n-2th pass because
         // in performAnalysisOnce, _currentPass is inc'ed before
         // calling EA again
         // (we really want to inline in the n-1th pass)
         manager()->setNumPassesCompleted(_maxPassNumber-2);
         _repeatAnalysis = true; // should already be true, just being paranoid
         }
      }

   // If we created non-contiguous allocations, and the method contains
   // catch blocks, the alias sets must be marked as non-valid.
   //
   if ((nonContiguousAllocations > 0) ||
       (tempsCreatedForColdEscapePoints > 0))
      {
      bool containsCatchBlocks = false;
      for (TR::CFGNode *node = comp()->getFlowGraph()->getFirstNode(); !containsCatchBlocks && node; node = node->getNext())
        if (!((TR::Block *)node)->getExceptionPredecessors().empty())
           containsCatchBlocks = true;

      if (containsCatchBlocks)
        optimizer()->setAliasSetsAreValid(false);
      }


   if (trace())
      {
      comp()->dumpMethodTrees("Trees after Escape Analysis");
      traceMsg(comp(), "Ending Escape Analysis");
      }

   return cost; // actual cost
   }

void TR_EscapeAnalysis::findIgnorableUses()
   {
   if (comp()->getOSRMode() != TR::voluntaryOSR)
      return;

   TR::NodeChecklist visited(comp());
   bool inOSRCodeBlock = false;

   // Gather all uses under fake prepareForOSR calls - they will be tracked as ignorable
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
       {
       if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
           inOSRCodeBlock = treeTop->getNode()->getBlock()->isOSRCodeBlock();
       else if (inOSRCodeBlock
                && treeTop->getNode()->getNumChildren() > 0
                && treeTop->getNode()->getFirstChild()->getOpCodeValue() == TR::call
                && treeTop->getNode()->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR)
          {
          TR::Node *callNode = treeTop->getNode()->getFirstChild();
          for (int i = 0; i < callNode->getNumChildren(); ++i)
             {
             markUsesAsIgnorable(callNode->getChild(i), visited);
             }
          }
       }
   }

void TR_EscapeAnalysis::markUsesAsIgnorable(TR::Node *node, TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;
   visited.add(node);
   if (trace())
      traceMsg(comp(), "Marking n%dn as an ignorable use\n", node->getGlobalIndex());
   _ignorableUses->set(node->getGlobalIndex());

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      markUsesAsIgnorable(child, visited);
      }
   }

void TR_EscapeAnalysis::findLocalObjectsValueNumbers()
   {
   TR::NodeChecklist visited(comp());
   TR::TreeTop *treeTop;

   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node    *node = treeTop->getNode();
      findLocalObjectsValueNumbers(node, visited);
      }
   }

void TR_EscapeAnalysis::findLocalObjectsValueNumbers(TR::Node *node, TR::NodeChecklist& visited)
   {
   if (visited.contains(node))
      return;
   visited.add(node);

   if (node->getOpCode().hasSymbolReference() &&
       node->getSymbolReference()->getSymbol()->isLocalObject())
      {
      _allLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(node));
      if (!node->escapesInColdBlock())
         {
         _nonColdLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(node));
         if (node->cannotTrackLocalUses())
            {
            if (!_notOptimizableLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(node)))
               {
               //dumpOptDetails(comp(), "Local object %p value number %d detected\n", node, _valueNumberInfo->getValueNumber(node));

               _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(node));
               }

            if (node->cannotTrackLocalStringUses())
               {
               if (!_notOptimizableLocalStringObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(node)))
                  {
                  //dumpOptDetails(comp(), "Local object %p value number %d detected\n", node, _valueNumberInfo->getValueNumber(node));

                  _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(node));
                  }
               }
            }
         }
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      findLocalObjectsValueNumbers(child, visited);
      }
   }


void TR_EscapeAnalysis::findCandidates()
   {
   TR::NodeChecklist visited (comp());
   int32_t     i;
   bool foundUserAnnotation=false;
   const char *className = NULL;
   for (_curTree = comp()->getStartTree(); _curTree; _curTree = _curTree->getNextTreeTop())
      {
      TR::Node    *node = _curTree->getNode();

      if (visited.contains(node))
         continue;
      visited.add(node);

      if (node->getOpCodeValue() == TR::BBStart)
         {
         _curBlock = node->getBlock();
         continue;
         }

      if (!node->getNumChildren())
         continue;

      node = node->getFirstChild();

      if (visited.contains(node))
         continue;
      visited.add(node);

      if (node->getOpCode().isNew() && node->isHeapificationAlloc())
         {
         if (trace())
            traceMsg(comp(), "Reject candidate %s n%dn [%p] because it is for heapification\n", node->getOpCode().getName(), node->getGlobalIndex(), node);
         continue;
         }

      // TODO-VALUETYPE:  If java.lang.Integer is ever made into a value type class, will
      //                  need to do TR::newvalue for dememoization instead
      //
      TR::SymbolReference *dememoizedMethodSymRef = NULL;
      TR::TreeTop         *dememoizedConstructorCall = NULL;
      if (!comp()->fej9()->callTargetsNeedRelocations() && node->getOpCodeValue() == TR::acall
         && node->getSymbol()->getMethodSymbol()->getRecognizedMethod() == TR::java_lang_Integer_valueOf
         && !node->isTheVirtualCallNodeForAGuardedInlinedCall()
         && manager()->numPassesCompleted() < _maxPassNumber)
         {
         // Dememoization: Let's look for a constructor call we could use instead
         //
         _dememoizationSymRef = node->getSymbolReference();

         if (trace()) traceMsg(comp(), "Attempt dememoize on %p\n", node);
         TR_OpaqueMethodBlock *constructor = comp()->getOption(TR_DisableDememoization)? NULL : comp()->fej9()->getMethodFromName("java/lang/Integer", "<init>", "(I)V");
         if (  constructor
            && performTransformation(comp(), "%sTry dememoizing %p\n", OPT_DETAILS, node))
            {
            // Replace this call with a allocation+constructor and hope for the best.
            //
            // NOTE!  This is not actually correct unless subsequent analysis
            // can prove it's correct; if it's not, we must rememoize this.
            //
            dememoizedMethodSymRef = node->getSymbolReference();
            TR::SymbolReference *constructorSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1,
               comp()->fej9()->createResolvedMethod(trMemory(), constructor), TR::MethodSymbol::Special);
            dememoizedConstructorCall = TR::TreeTop::create(comp(), _curTree,
               TR::Node::create(TR::treetop, 1,
                  TR::Node::createWithSymRef(TR::call, 2, 2,
                     node,
                     node->getFirstChild(),
                     constructorSymRef)));
            node->getFirstChild()->decReferenceCount();
            node->setAndIncChild(0,
               TR::Node::createWithSymRef(node, TR::loadaddr, 0,
                  comp()->getSymRefTab()->findOrCreateClassSymbol(comp()->getMethodSymbol(), -1,
                     comp()->fej9()->getClassFromSignature("java/lang/Integer", 17, comp()->getCurrentMethod(), true))));
            TR::Node::recreate(node, TR::New);
            if (!_dememoizedAllocs.find(node))
               _dememoizedAllocs.add(node);
            node->setSymbolReference(comp()->getSymRefTab()->findOrCreateNewObjectSymbolRef(comp()->getMethodSymbol()));
            }
         }


      if (node->getOpCodeValue() != TR::New &&
          node->getOpCodeValue() != TR::newvalue &&
          node->getOpCodeValue() != TR::newarray &&
          node->getOpCodeValue() != TR::anewarray)
         {
         continue;
         }

      if (_disableValueTypeStackAllocation && (node->getOpCodeValue() == TR::newvalue))
         {
         if (trace())
            {
            traceMsg(comp(), "Reject candidate %s n%dn [%p] because value type stack allocation is disabled\n", node->getOpCode().getName(), node->getGlobalIndex(), node);
            }
         continue;
         }

      static char *noEscapeArrays = feGetEnv("TR_NOESCAPEARRAY");
      if (noEscapeArrays)
         {
         if (node->getOpCodeValue() != TR::New && node->getOpCodeValue() != TR::newvalue)
            continue;
         }


      // Found a "new" opcode. See if it is a candidate for local allocation.
      //
      //
      bool inAColdBlock = false;
      if (_curBlock->isCold() ||
          _curBlock->isCatchBlock() ||
         (_curBlock->getFrequency() == (MAX_COLD_BLOCK_COUNT+1)))
         inAColdBlock = true;

      if (trace())
         {
         if (node->getOpCodeValue() == TR::New)
            {
            const char *className = getClassName(node->getFirstChild());
            traceMsg(comp(), "Found [%p] new %s\n", node,
                     className ? className : "<Missing class name>");
            }
         else if (node->getOpCodeValue() == TR::newvalue)
            {
            const char *className = getClassName(node->getFirstChild());
            traceMsg(comp(), "Found [%p] newvalue of type %s\n", node, className ? className : "<Missing value type class name>");
            }
         else if (node->getOpCodeValue() == TR::newarray)
            {
            traceMsg(comp(), "Found [%p] newarray of type %d\n", node, node->getSecondChild()->getInt());
            }
         else
            {
            const char *className = getClassName(node->getSecondChild());
            traceMsg(comp(), "Found [%p] anewarray %s\n", node,
                     className ? className : "<Missing class name>");
            }
         }

      foundUserAnnotation=false;

      // Make this a candidate for local allocation and/or desynchronization.
      // Checks for escape are done after the candidates have been collected.
      //
      TR_OpaqueClassBlock *classInfo = 0;
      Candidate *candidate = createCandidateIfValid(node, classInfo,foundUserAnnotation);
      if (!candidate)
         continue;
      if (dememoizedConstructorCall)
         {
         candidate->_dememoizedMethodSymRef    = dememoizedMethodSymRef;
         candidate->_dememoizedConstructorCall = dememoizedConstructorCall;
         //candidate->setObjectIsReferenced();
         candidate->getCallSites()->add(dememoizedConstructorCall);
         }

      // candidate->_size is:
      //     -1 if it is not a valid candidate.
      //     0 if it is a valid candidate for desynchronizing only.
      //     allocation size otherwise.
      //
      candidate->setLocalAllocation(_createStackAllocations && (candidate->_size > 0));
      if (trace())
         traceMsg(comp(), "4 setting local alloc %p to %s\n", candidate->_node, candidate->isLocalAllocation()? "true":"false");

      if(foundUserAnnotation)
         {
         candidate->setForceLocalAllocation(true);
         candidate->setObjectIsReferenced();
        if (trace())
           traceMsg(comp(), "   Force [%p] to be locally allocated due to annotation of %s\n", node, className);
         }

      if (candidate->isLocalAllocation())
         {
         if (node->getSymbolReference() == _newObjectNoZeroInitSymRef ||
             node->getSymbolReference() == _newValueSymRef ||
             node->getSymbolReference() == _newArrayNoZeroInitSymRef ||
             node->getSymbolReference() == _aNewArrayNoZeroInitSymRef)
            {
            candidate->setExplicitlyInitialized();
            }

         if (blockIsInLoop(_curBlock))
            candidate->setInsideALoop();

         if (inAColdBlock)
            candidate->setInAColdBlock(true);
         }


      _candidates.add(candidate);
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after finding candidates");
      }
   }


Candidate *TR_EscapeAnalysis::createCandidateIfValid(TR::Node *node, TR_OpaqueClassBlock *&classInfo,bool foundUserAnnotation)
   {
   // If user has annotated this objects class, force it to be on the stack
   if(!foundUserAnnotation)
      {
      // The only case where an object allocation can automatically escape is when
      // it implements the "Runnable" interface. Check for that first and dismiss
      // it immediately. If the class is unresolved, we don't know so we have to
      // assume the worst.
      //
      if (node->getOpCodeValue() == TR::New || node->getOpCodeValue() == TR::newvalue)
         {
         TR::Node *classNode = node->getFirstChild();
         if (classNode->getOpCodeValue() != TR::loadaddr)
            {
            if (trace())
               traceMsg(comp(), "   Node [%p] failed: child is not TR::loadaddr\n", node);
            return NULL;
            }

         if (classNode->getSymbolReference()->isUnresolved())
            {
            if (trace())
               traceMsg(comp(), "   Node [%p] failed: class is unresolved\n", node);
            return NULL;
            }

         TR::StaticSymbol *classSym = classNode->getSymbol()->castToStaticSymbol();

         // Removed assume, does not make sense when doing aot -- ALI
         // TR_ASSERT(comp()->getRunnableClassPointer(), "Need access to java/lang/Runnable");
         if (comp()->getRunnableClassPointer() &&
             comp()->fej9()->isInstanceOf((TR_OpaqueClassBlock *)classSym->getStaticAddress(), comp()->getRunnableClassPointer(), true) == TR_yes)
            {
            if (trace())
               {
               const char *className = getClassName(classNode);
               traceMsg(comp(), "secs Class %s implements Runnable in %s\n",
                  className ? className : "<Missing class name>",
                  comp()->signature());
               traceMsg(comp(), "   Node [%p] failed: class implements the Runnable interface\n", node);
               }
            return NULL;
            }
         }
      // Don't convert double-word arrays if platform does not have double-word aligned stacks
      // will handle stack alignment later
      else if (!comp()->cg()->getHasDoubleWordAlignedStack() &&
                node->getOpCodeValue() == TR::newarray && !comp()->getOption(TR_EnableSIMDLibrary))
         {
         TR::Node *typeNode = node->getSecondChild();
         if (typeNode->getInt() == 7 || typeNode->getInt() == 11)
            {
            if (trace())
               traceMsg(comp(), "   Node [%p] failed: double-size array\n", node);
            return NULL;
            }
         }
      }


   if (comp()->cg()->getSupportsStackAllocationOfArraylets())
      {
      if (node->getOpCodeValue() != TR::New && node->getOpCodeValue() != TR::newvalue)
         {
         if (trace())
            traceMsg(comp(), "   Node [%p] failed: arraylet\n", node);

         return NULL;
         }
      }

   bool profileOnly = false;

   // See if the VM thinks it is valid to eliminate this allocation node. If not
   // the allocation can't be made into a stack allocation. If it is an object
   // allocation we can still look for desynchronization opportunities.
   //
   int32_t size = comp()->canAllocateInlineOnStack(node, classInfo);

   if (((node->getOpCodeValue() == TR::newarray) || (node->getOpCodeValue() == TR::anewarray)) &&
       (node->getFirstChild()->getOpCodeValue() == TR::iconst))
      {
      if (node->getFirstChild()->getInt() == 0)
         return NULL;
      }

   if (classInfo &&
       !TR::Compiler->cls.sameClassLoaders(comp(), classInfo, comp()->getJittedMethodSymbol()->getResolvedMethod()->containingClass()) &&
       !comp()->fej9()->isClassLoadedBySystemClassLoader(classInfo))
      return NULL;

   if (size <= 0)
      {
      if (trace())
         traceMsg(comp(), "   Node [%p] failed: VM can't skip allocation (code %d, class %p)\n", node, size, classInfo);

      if (  size == 0
         && classInfo
         && (manager()->numPassesCompleted() == 0)
         && optimizer()->isEnabled(OMR::profiledNodeVersioning)
         && !_curBlock->isCold())
         {
         TR::Node *numElementsNode = NULL;
         switch (node->getOpCodeValue())
            {
            case TR::newarray:
            case TR::anewarray:
               numElementsNode = node->getFirstChild();
               break;
            case TR::New:
            case TR::newvalue:
            case TR::multianewarray:
               // Can't do anything with these yet
               break;
            default:
                break;
            }

         TR::Recompilation *recomp        = comp()->getRecompilationInfo();
         TR_ValueProfiler *valueProfiler = recomp? recomp->getValueProfiler() : NULL;

         if (  numElementsNode
            && valueProfiler
            && performTransformation(comp(), "%sContinue analyzing %s node %s for size-profiling opportunity\n", OPT_DETAILS,
                                       node->getOpCode().getName(),
                                       comp()->getDebug()->getName(node)))
            {
            profileOnly = true;
            size = TR::Compiler->om.contiguousArrayHeaderSizeInBytes(); // Must be at least this big
            }
         else
            {
            return NULL;
            }
         }
      else if ((node->getOpCodeValue() == TR::New || node->getOpCodeValue() == TR::newvalue)
               && classInfo)
         size = 0;
      else
         return NULL;
      }
   else
      {
      // j/l/Reference objects are no longer enqueued by a native call in the constructor.
      // This native was preventing j/l/Reference objects from being stack allocated, but
      // we now need to explicitly prevent j/l/Reference (and its subclasses) from being
      // stack allocated because the GC can't discover them during scanning (this is non-
      // trivial to do apparently).
      //
      TR_OpaqueClassBlock *jlReference = comp()->getReferenceClassPointer();
      TR_OpaqueClassBlock *jlObject = comp()->getObjectClassPointer();
      TR_OpaqueClassBlock *currentClass = classInfo;

      while (currentClass && currentClass != jlObject)
         {
         if (currentClass == jlReference)
            {
            if (trace())
               traceMsg(comp(), "   Node [%p] failed: class %p is subclass of j/l/r/Reference\n", node, classInfo);

            return NULL;
            }
         else
            {
            currentClass = comp()->fej9()->getSuperClass(currentClass);
            }
         }
      }

   Candidate *result = NULL;
   result = new (trStackMemory()) Candidate(node, _curTree, _curBlock, size, classInfo, comp());

   result->setProfileOnly(profileOnly);
   return result;
   }



bool TR_EscapeAnalysis::isEscapePointCold(Candidate *candidate, TR::Node *node)
   {
   static const char *disableColdEsc = feGetEnv("TR_DisableColdEscape");
   if (!disableColdEsc &&
       (_inColdBlock ||
        (candidate->isInsideALoop() &&
         (candidate->_block->getFrequency() > 4*_curBlock->getFrequency()))) &&
       (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue))
      return true;

   return false;
   }


void TR_EscapeAnalysis::checkDefsAndUses()
   {
   Candidate *candidate, *next;

   gatherUsesThroughAselect();

   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      TR::Node   *node          = candidate->_node;
      int32_t    newVN         = _valueNumberInfo->getValueNumber(node);
      candidate->_valueNumbers = new (trStackMemory()) TR_Array<int32_t>(trMemory(), 8, false, stackAlloc);
      candidate->_valueNumbers->add(newVN);

      // Accumulate the set of value numbers that can be reached by this
      // allocation. This also checks that it is valid to allocate on the stack.
      //
      if (candidate->isInsideALoop())
         {
         if (_otherDefsForLoopAllocation)
            _otherDefsForLoopAllocation->empty();
         else
            _otherDefsForLoopAllocation= new (trStackMemory()) TR_BitVector(_useDefInfo->getNumDefNodes(), trMemory(), stackAlloc);
         }

      if (comp()->getOptions()->realTimeGC() &&
          comp()->compilationShouldBeInterrupted(ESC_CHECK_DEFSUSES_CONTEXT))
         {
         comp()->failCompilation<TR::CompilationInterrupted>("interrupted in Escape Analysis");
         }

      if (checkDefsAndUses(node, candidate))
         {
         if (candidate->_valueNumbers->size() > 1)
            {
            candidate->setMustBeContiguousAllocation();
            if (trace())
               traceMsg(comp(), "   Make [%p] contiguous because its uses can be reached from other defs\n", candidate->_node);
            }
         }
      else
         {
         candidate->setLocalAllocation(false);
         if (trace())
            traceMsg(comp(), "5 setting local alloc %p to false\n", candidate->_node);
         }
      }

   _vnTemp = new (trStackMemory()) TR_BitVector( optimizer()->getValueNumberInfo()->getNumberOfNodes(), trMemory(), stackAlloc, notGrowable);
   _vnTemp2 = new (trStackMemory()) TR_BitVector(optimizer()->getValueNumberInfo()->getNumberOfNodes(), trMemory(), stackAlloc, notGrowable);

   TR::TreeTop *tt = comp()->getStartTree();
   for (; tt; tt = tt->getNextTreeTop())
      {
      bool storeOfObjectIntoField = false;
      TR::Node *node = tt->getNode();
      if (!node->getOpCode().isStore())
         {
         if (node->getNumChildren() > 0)
            node = node->getFirstChild();
         }

      bool storeIntoOtherLocalObject = false;
      int32_t baseChildVN = -1;
      if (node->getOpCode().isStoreIndirect() ||
          (node->getOpCodeValue() == TR::arraycopy))
         {
         TR::Node *baseObject = node;

         if (node->getSymbol()->isArrayShadowSymbol() &&
             node->getFirstChild()->getOpCode().isArrayRef())
            baseObject = node->getFirstChild();
         else if (node->getOpCodeValue() == TR::arraycopy)
            {
            if (node->getNumChildren() == 5)
               baseObject = node->getChild(3);
            else if (node->getFirstChild()->getOpCode().isArrayRef())
               baseObject = node->getFirstChild();
            }

         if (node->getOpCode().isStoreIndirect() &&
             (baseObject->getFirstChild() == node->getSecondChild()))
            storeOfObjectIntoField = true;
         else
            {
            TR::Node *baseChild = baseObject;

            if ((node->getOpCodeValue() != TR::arraycopy) ||
                (node->getNumChildren() != 5))
                baseChild = baseObject->getFirstChild();

            baseChild = resolveSniffedNode(baseChild);

            if ((baseChild && (baseChild->getOpCodeValue() == TR::loadaddr) &&
                baseChild->getSymbolReference()->getSymbol()->isAuto() &&
                baseChild->getSymbolReference()->getSymbol()->isLocalObject())
               )
               {
               baseChildVN = _valueNumberInfo->getValueNumber(baseChild);
               if (node->getOpCodeValue() == TR::arraycopy)
                  {
                  _notOptimizableLocalObjectsValueNumbers->set(baseChildVN);
                  _notOptimizableLocalStringObjectsValueNumbers->set(baseChildVN);
                  storeOfObjectIntoField = false;
                  if (trace())
                     traceMsg(comp(), "Reached 0 with baseChild %p VN %d\n", baseChild, baseChildVN);
                  }
               else
                  {
                  if (!baseChild->cannotTrackLocalUses())
                     {
                     storeOfObjectIntoField = true;
                     storeIntoOtherLocalObject = true;
                     if (trace())
                        traceMsg(comp(), "Reached 1 with baseChild %p VN %d\n", baseChild, baseChildVN);
                     }
                  else
                     {
                     _notOptimizableLocalObjectsValueNumbers->set(baseChildVN);
                     _notOptimizableLocalStringObjectsValueNumbers->set(baseChildVN);
                     storeOfObjectIntoField = false;
                     }
                  }
               }
            else if (baseChild && _useDefInfo)
               {
               uint16_t baseIndex = baseChild->getUseDefIndex();
               if (_useDefInfo->isUseIndex(baseIndex))
                  {
                  TR_UseDefInfo::BitVector defs(comp()->allocator());
                  _useDefInfo->getUseDef(defs, baseIndex);
                  if (!defs.IsZero())
                     {
                     TR_UseDefInfo::BitVector::Cursor cursor(defs);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = cursor;

                        if (defIndex < _useDefInfo->getFirstRealDefIndex())
                           {
                           storeOfObjectIntoField = false;
                           break;
                           }

                        TR::TreeTop *defTree = _useDefInfo->getTreeTop(defIndex);
                        TR::Node *defNode = defTree->getNode();

                        if (defNode &&
                            (defNode->getOpCodeValue() == TR::astore) &&
                            (defNode->getNumChildren() > 0))
                           {
                           TR::Node *defChild = defNode->getFirstChild();

                           if (defChild && (defChild->getOpCodeValue() == TR::loadaddr) &&
                               defChild->getSymbolReference()->getSymbol()->isAuto() &&
                               defChild->getSymbolReference()->getSymbol()->isLocalObject() &&
                               !defChild->cannotTrackLocalUses())
                              {
                              if (node->getOpCode().isStoreIndirect() &&
                                  (_valueNumberInfo->getValueNumber(defChild) == _valueNumberInfo->getValueNumber(baseChild)))
                                 {
                                 baseChildVN = _valueNumberInfo->getValueNumber(baseChild);
                                 storeOfObjectIntoField = true;
                                 storeIntoOtherLocalObject = true;
                                 }
                              else
                                 {
                                 _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(baseChild));
                                 _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(defChild));
                                 _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(baseChild));
                                 _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(defChild));
                                 storeOfObjectIntoField = false;
                                 break;
                                 }
                              }
                           else
                              {
                              _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(baseChild));
                              _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(defChild));
                              _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(baseChild));
                              _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(defChild));
                              storeOfObjectIntoField = false;
                              break;
                              }
                           }
                        }
                     }
                  }
               }
            }
         }

      if (storeOfObjectIntoField)
        {
            int32_t valueNumber = _valueNumberInfo->getValueNumber(node->getSecondChild());
            Candidate *candidate, *next;
            bool foundAccess = false;
            for (candidate = _candidates.getFirst(); candidate; candidate = next)
               {
               next = candidate->getNext();
               if (usesValueNumber(candidate, valueNumber))
                  {
                  TR::NodeChecklist visited (comp());
                  TR::TreeTop *cursorTree = comp()->getStartTree();
                  for (; cursorTree; cursorTree = cursorTree->getNextTreeTop())
                     {
                     TR::Node *cursorNode = cursorTree->getNode();
                     if (!storeIntoOtherLocalObject)
                        {
                        if (collectValueNumbersOfIndirectAccessesToObject(cursorNode, candidate, node, visited))
                           foundAccess = true;
                        }
                     else
                        {
                        if (collectValueNumbersOfIndirectAccessesToObject(cursorNode, candidate, node, visited, baseChildVN))
                           foundAccess = true;
                        }
                     }
                  }
               }
        }


      if (node->getOpCode().isCall() &&
          (node->getSymbol()->getResolvedMethodSymbol()) &&
          (node->getReferenceCount() > 1) &&
          (node->getNumChildren() > 0))
         {
         TR::ResolvedMethodSymbol *methodSymbol = node->getSymbol()->getResolvedMethodSymbol();
         switch (methodSymbol->getRecognizedMethod())
            {
            case TR::java_lang_Throwable_fillInStackTrace:
            case TR::java_math_BigDecimal_possibleClone:
               {
               int32_t firstArgIndex = node->getFirstArgumentIndex();
               int32_t nodeVN = _valueNumberInfo->getValueNumber(node->getChild(firstArgIndex));
               for (Candidate *candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
                  {
                  if (usesValueNumber(candidate, nodeVN))
                     {
                     // Remember the value number of the fillInStackTrace call itself
                     // It's return value can be assigned to a local and the local can be used
                     // which would mean we cannot eliminate the fillInStackTrace.
                     //
                     if (methodSymbol && (!methodSymbol->getResolvedMethod()->virtualMethodIsOverridden() || !node->getOpCode().isIndirect()))
                        {
                        candidate->_valueNumbers->add(_valueNumberInfo->getValueNumber(node));
                        }
                     }
                  }
               }
            default:
                break;
            }
         }
      }
   }

void TR_EscapeAnalysis::printUsesThroughAselect(void)
   {
   if (trace())
      {
      if (_nodeUsesThroughAselect)
         {
         traceMsg(comp(), "\nNodes used through aselect operations\n");

         for (auto mi = _nodeUsesThroughAselect->begin(); mi != _nodeUsesThroughAselect->end(); mi++)
            {
            TR::Node *key = mi->first;
            int32_t nodeIdx = key->getGlobalIndex();

            traceMsg(comp(), "   node [%p] n%dn is used by {", key, nodeIdx);

            bool first = true;

            for (auto di = mi->second->begin(), end = mi->second->end(); di != end; di++)
               {
               TR::Node *aselectNode = *di;
               traceMsg(comp(), "%s[%p] n%dn", (first ? "" : ", "), aselectNode,
                        aselectNode->getGlobalIndex());
               first = false;
               }

            traceMsg(comp(), "}\n");
            }
         }
      else
         {
         traceMsg(comp(), "\nNo nodes used through aselect operations\n");
         }
      }
   }

void TR_EscapeAnalysis::gatherUsesThroughAselect(void)
   {
   TR::NodeChecklist visited(comp());
   TR::TreeTop *tt = comp()->getStartTree();

   for (; tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      gatherUsesThroughAselectImpl(node, visited);
      }

   if (trace())
      {
      printUsesThroughAselect();
      }
   }

void TR_EscapeAnalysis::gatherUsesThroughAselectImpl(TR::Node *node, TR::NodeChecklist& visited)
   {
   if (visited.contains(node))
      {
      return;
      }
   visited.add(node);

   for (int32_t i=0; i<node->getNumChildren(); i++)
      {
      gatherUsesThroughAselectImpl(node->getChild(i), visited);
      }

   // If this is an aselect operation, for each of its child operands (other than
   // the condition) add the aselect node to the array of nodes that use that child
   if (node->getOpCode().isSelect() && node->getDataType() == TR::Address)
      {
      associateAselectWithChild(node, 1);
      associateAselectWithChild(node, 2);
      }
   }

void TR_EscapeAnalysis::associateAselectWithChild(TR::Node *aselectNode, int32_t idx)
   {
   TR::Region &stackMemoryRegion = trMemory()->currentStackRegion();
   TR::Node *child = aselectNode->getChild(idx);

   NodeDeque *currChildUses;

   if (NULL == _nodeUsesThroughAselect)
      {
      _nodeUsesThroughAselect =
            new (trStackMemory()) NodeToNodeDequeMap((NodeComparator()),
                                                     NodeToNodeDequeMapAllocator(stackMemoryRegion));
      }

   auto search = _nodeUsesThroughAselect->find(child);
   bool nodeAlreadyMapsToAselect = false;

   if (_nodeUsesThroughAselect->end() != search)
      {
      currChildUses = search->second;

      // Does NodeDeque already contain this aselect node?
      nodeAlreadyMapsToAselect =
            (std::find(search->second->begin(), search->second->end(),
                       aselectNode) != search->second->end());
      }
   else
      {
      currChildUses = new (trStackMemory()) NodeDeque(stackMemoryRegion);
      (*_nodeUsesThroughAselect)[child] = currChildUses;
      }

   if (!nodeAlreadyMapsToAselect)
      {
      currChildUses->push_back(aselectNode);
      }
   }

bool TR_EscapeAnalysis::collectValueNumbersOfIndirectAccessesToObject(TR::Node *node, Candidate *candidate, TR::Node *indirectStore, TR::NodeChecklist& visited, int32_t baseChildVN)
   {
   if (visited.contains(node))
      return false;
   visited.add(node);

   bool foundAccess = false;

   if (node->getOpCode().isLoadIndirect())
      {
      bool sameSymbol = false;
      if (node->getSymbolReference()->getReferenceNumber() == indirectStore->getSymbolReference()->getReferenceNumber())
         sameSymbol = true;
      else if (indirectStore->getSymbolReference()->sharesSymbol())
         {
         if (indirectStore->getSymbolReference()->getUseDefAliases().contains(node->getSymbolReference(), comp()))
            sameSymbol = true;
         }

      if (trace())
         traceMsg(comp(), "store node %p load node %p candidate %p baseChildVN %d\n", indirectStore, node, candidate->_node, baseChildVN);

      if (sameSymbol)
         {
         TR::Node *base = node->getFirstChild();

         if (/* node->getSymbol()->isArrayShadowSymbol() && */
             base->getOpCode().isArrayRef())
            base = base->getFirstChild();

         int32_t baseVN = _valueNumberInfo->getValueNumber(base);

         //traceMsg(comp(), "store node %p load node %p candidate %p baseChildVN %d baseVN %d\n", indirectStore, node, candidate->_node, baseChildVN, baseVN);

         if (candidate->_valueNumbers)
            {
            if ((baseChildVN == -1) && usesValueNumber(candidate, baseVN))
               {
               candidate->_valueNumbers->add(_valueNumberInfo->getValueNumber(node));
               if (checkDefsAndUses(node, candidate))
                  foundAccess = true;
               else
                  {
                  TR::Node *resolvedBaseObject = resolveSniffedNode(indirectStore->getFirstChild());
                  if (resolvedBaseObject)
                     {
                     _notOptimizableLocalObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(resolvedBaseObject));
                     _notOptimizableLocalStringObjectsValueNumbers->set(_valueNumberInfo->getValueNumber(resolvedBaseObject));
                     }
                  }
               }
            else if (baseChildVN != -1)
               {
               if (baseChildVN == baseVN)
                  {
                  candidate->_valueNumbers->add(_valueNumberInfo->getValueNumber(node));
                  if (checkDefsAndUses(node, candidate))
                     foundAccess = true;
                  else
                     {
                     _notOptimizableLocalObjectsValueNumbers->set(baseChildVN);
                     _notOptimizableLocalStringObjectsValueNumbers->set(baseChildVN);
                     }
                  }
               else
                  {
                  TR::Node *storeBase = indirectStore->getFirstChild();
                  if (/* indirectStore->getSymbol()->isArrayShadowSymbol() && */
                      storeBase->getOpCode().isArrayRef())
                     storeBase = storeBase->getFirstChild();


                  if (base->getOpCode().hasSymbolReference() &&  base->getSymbolReference()->getSymbol()->isAuto())
                     {
                     if (_useDefInfo)
                        {
                        uint16_t baseIndex = base->getUseDefIndex();
                        //uint16_t storeBaseIndex = storeBase->getUseDefIndex();
                        TR_UseDefInfo::BitVector baseDefs(comp()->allocator());
                        _useDefInfo->getUseDef(baseDefs, baseIndex);
                        //TR_UseDefInfo::BitVector storeBaseDefs(comp()->allocator());
                        //_useDefInfo->getUseDef(storeBaseDefs, storeBaseIndex);
                        //traceMsg(comp(), "store base index %d store base %p base index %p base %p\n", storeBaseIndex, storeBase, baseIndex, base);

                        _vnTemp->set(_valueNumberInfo->getValueNumber(storeBase));
                        while (*_vnTemp2 != *_vnTemp)
                           {
                           _vnTemp->print(comp());
                           *_vnTemp2 = *_vnTemp;
                           int32_t i;
                           for (i = _useDefInfo->getNumDefOnlyNodes()-1; i >= 0; --i)
                              {
                              int32_t useDefIndex = i + _useDefInfo->getFirstDefIndex();
                              TR::Node *defNode = _useDefInfo->getNode(useDefIndex);
                              //traceMsg(comp(), "def node %p\n", defNode);

                              if (defNode && defNode->getOpCode().isStore())
                                 {
                                 if (_vnTemp->get(_valueNumberInfo->getValueNumber(defNode)))
                                    {
                                    TR_UseDefInfo::BitVector usesOfThisDef(comp()->allocator());
                                    _useDefInfo->getUsesFromDef(usesOfThisDef, defNode->getUseDefIndex()+_useDefInfo->getFirstDefIndex());
                                    if (!usesOfThisDef.IsZero())
                                       {
                                       TR_UseDefInfo::BitVector::Cursor cursor(usesOfThisDef);
                                       for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                                          {
                                          int32_t useIndex = cursor;
                                          TR::Node *useNode = _useDefInfo->getNode(useIndex+_useDefInfo->getFirstUseIndex());
                                          int32_t useNodeVN = _valueNumberInfo->getValueNumber(useNode);
                                          //traceMsg(comp(), "use node %p vn %d\n", useNode, useNodeVN);

                                          _vnTemp->set(useNodeVN);
                                          }
                                       }
                                    }
                                 }
                              }
                           }

                        TR_UseDefInfo::BitVector::Cursor cursor(baseDefs);
                        for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                           {
                           int32_t defIndex = _useDefInfo->getFirstDefIndex() + (int32_t) cursor;
                           //TODO: Temporary fix to overcome case when defIndex = 0
                           if (defIndex < _useDefInfo->getFirstRealDefIndex())
                              continue;

                           TR::Node *defNode = _useDefInfo->getNode(defIndex);
                           if (_vnTemp->get(_valueNumberInfo->getValueNumber(defNode)))
                              {
                              candidate->_valueNumbers->add(_valueNumberInfo->getValueNumber(node));
                              break;
                              }
                           }
                        }
                     }

                  _notOptimizableLocalObjectsValueNumbers->set(baseChildVN);
                  _notOptimizableLocalStringObjectsValueNumbers->set(baseChildVN);
                  }
               }
            }
         }
      }


   int32_t i;
   for (i=0;i<node->getNumChildren(); i++)
      {
      if (collectValueNumbersOfIndirectAccessesToObject(node->getChild(i), candidate, indirectStore, visited, baseChildVN))
         foundAccess = true;
      }
   return foundAccess;
   }


bool TR_EscapeAnalysis::checkUsesThroughAselect(TR::Node *node, Candidate *candidate)
   {
   bool returnValue = true;

   if (_nodeUsesThroughAselect)
      {
      auto search = _nodeUsesThroughAselect->find(node);

      // Is this node referenced directly by any aselect nodes?
      if (_nodeUsesThroughAselect->end() != search)
         {
         for (auto di = search->second->begin(), end = search->second->end(); di != end; di++)
            {
            TR::Node* aselectNode = *di;
            int32_t aselectVN = _valueNumberInfo->getValueNumber(aselectNode);
            int32_t i;

            // Check whether this aselect has already been accounted for with this candidate
            for (i = candidate->_valueNumbers->size()-1; i >= 0; i--)
               {
               if (candidate->_valueNumbers->element(i) == aselectVN)
                  {
                  break;
                  }
               }

            // If this aselect has not been accounted for with this candidate, check for its uses
            if (i < 0)
               {
               candidate->_valueNumbers->add(aselectVN);

               if (trace())
                  {
                  traceMsg(comp(), "   Checking uses of node %p through aselect operation %p for candidate %p\n", node, aselectNode, candidate->_node);
                  }

               if (!checkDefsAndUses(aselectNode, candidate))
                  {
                  returnValue = false;
                  }
               }
            }
         }
      }

   return returnValue;
   }


bool TR_EscapeAnalysis::checkDefsAndUses(TR::Node *node, Candidate *candidate)
   {
   TR::Node *next;
   _useDefInfo->buildDefUseInfo();
   bool returnValue = true;

   if (_nodeUsesThroughAselect && !checkUsesThroughAselect(node, candidate))
      {
      returnValue = false;
      }

   for (next = _valueNumberInfo->getNext(node); next != node; next = _valueNumberInfo->getNext(next))
      {
      int32_t udIndex = next->getUseDefIndex();

      if (_useDefInfo->isDefIndex(udIndex))
         {
         if (next->getOpCode().isStore() && next->getSymbol()->isAutoOrParm())
            {
            TR::SymbolReference *symRef = next->getSymbolReference();
            if (!candidate->getSymRefs()->find(symRef))
               candidate->addSymRef(symRef);

            // Find all uses of this def and see if their value numbers are
            // already known in the array of value numbers.
            // If not, add the new value number and recurse to find value
            // numbers of nodes reachable from this use.
            //

            TR_UseDefInfo::BitVector defUse(comp()->allocator());
            _useDefInfo->getUsesFromDef(defUse, udIndex-_useDefInfo->getFirstDefIndex());
            if (!defUse.IsZero())
               {
               TR_UseDefInfo::BitVector::Cursor cursor(defUse);
               for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                  {
                  int32_t useIndex = cursor;
                  TR::Node *useNode = _useDefInfo->getNode(useIndex+_useDefInfo->getFirstUseIndex());

                  // Only add this value number if it's not to be ignored
                  if (_ignorableUses->get(useNode->getGlobalIndex()))
                     {
                     continue;
                     }

                  int32_t useNodeVN = _valueNumberInfo->getValueNumber(useNode);
                  int32_t i;

                  for (i = candidate->_valueNumbers->size()-1; i >= 0; i--)
                     {
                     if (candidate->_valueNumbers->element(i) == useNodeVN)
                        {
                        break;
                        }
                     }

                  if (i < 0)
                     {
                     candidate->_valueNumbers->add(useNodeVN);

                     if (candidate->isInsideALoop())
                        {
                        static char *p = feGetEnv("TR_NoLoopAlloc");
                        if (!p)
                           {
                           // For an allocation inside a loop we must make sure
                           // that two generations of the allocation can't
                           // co-exist.
                           // There are 2 ways this can happen - check them.
                           //
                           if (trace())
                              traceMsg(comp(), "   Look at other defs for use node %p of candidate %p\n", useNode, candidate->_node);
                           ////_otherDefsForLoopAllocation->set(udIndex);

                           if (!checkOverlappingLoopAllocation(useNode, candidate))
                              {
                              if (trace())
                                 traceMsg(comp(), "   Make [%p] non-local because it overlaps with use [%p]\n", candidate->_node, useNode);
                              /////printf("secs Overlapping loop allocation in %s\n", comp()->signature());
                              returnValue = false;
                              }
                           if (!checkOtherDefsOfLoopAllocation(useNode, candidate, (next->getFirstChild() == candidate->_node)))
                              {
                              if (trace())
                                 traceMsg(comp(), "   Make [%p] non-local because multiple defs to node [%p]\n", candidate->_node, useNode);
                              returnValue = false;
                              }
                           }
                        else
                           returnValue = false;
                        }
                     if (!checkDefsAndUses(useNode, candidate))
                        returnValue = false;
                     }
                  }
               }
            }
         }

      if (_useDefInfo->isUseIndex(udIndex))
         {
         if (_nodeUsesThroughAselect && !checkUsesThroughAselect(next, candidate))
            {
            returnValue = false;
            }
         }
      }
   return returnValue;
   }

bool TR_EscapeAnalysis::checkOtherDefsOfLoopAllocation(TR::Node *useNode, Candidate *candidate, bool isImmediateUse)
   {
   // The allocation is inside a loop and a use has been found that has other
   // defs too. Find all the possible defs for this use and make sure none of
   // them lead back to the allocation. If they do, it means that generations
   // of the allocation from different loop iterations may be alive at the same
   // time, so the allocation must be done from the heap and not the stack.
   //
   // In some limited cases, we can be sure that an object from a prior loop iteration
   // was not live at the same time as an object from the next loop iteration without expensive analysis.
   // One such "special" case is when all defs for uses reached by our candidate for stack allocation
   // were fed by allocations; in this case it's easy to see that it was not an object from a prior iteration
   // since it is a fresh allocation being done at that program point.
   //
   // There is one other special case dealt with in the code below related to a java/lang/Integer cache
   // where again it's trivial to prove that the value cannot be a candidate allocation from a prior loop iteration
   //
   // There may be other such examples that can be added in the future, e.g. if the value is an already stack allocated
   // object from a prior pass of escape analysis, it obviously cannot be a candidate for stack allocation in this pass.
   //
   int32_t useIndex = useNode->getUseDefIndex();
   if (useIndex <= 0)
      return true;

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   _useDefInfo->getUseDef(defs, useIndex);
   TR_UseDefInfo::BitVector::Cursor cursor(defs);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t defIndex = cursor;
      if (defIndex < _useDefInfo->getFirstRealDefIndex() /* || _otherDefsForLoopAllocation->get(defIndex) */)
         {
         continue;
         }

      bool seenOtherDef = false;
      if (_otherDefsForLoopAllocation->get(defIndex))
         seenOtherDef = true;

      // Ignore the def that is the one that caused us to look at this use in
      // the first place
      //
      TR::Node *defNode = _useDefInfo->getNode(defIndex);
      if (!seenOtherDef &&
          isImmediateUse &&
          _valueNumberInfo->getValueNumber(defNode) == _valueNumberInfo->getValueNumber(candidate->_node))
         ///_valueNumberInfo->getValueNumber(defNode) == _valueNumberInfo->getValueNumber(useNode))
         {
         if (trace())
            traceMsg(comp(), "      Ignoring def node [%p] for use node [%p]\n", defNode, useNode);
         continue;
         }

      _otherDefsForLoopAllocation->set(defIndex);

      if (trace())
         traceMsg(comp(), "      Look at def node [%p] for use node [%p]\n", defNode, useNode);

      bool allnewsonrhs = checkAllNewsOnRHSInLoopWithAliasing(defIndex, useNode, candidate);


      if (!allnewsonrhs &&
          !(defNode->getOpCode().isStoreDirect() &&
            ((defNode->getFirstChild()->getOpCodeValue() == TR::aconst) ||
             (defNode->getFirstChild()->getOpCode().isLoadVar() && // load from a static or array shadow or shadow (field) based off a non local object are fine since such memory locations would imply that the object being pointed at escaped already (meaning there would be another escape point anyway)
             (defNode->getFirstChild()->getSymbol()->isStatic() ||
             (defNode->getFirstChild()->getSymbol()->isShadow() &&
             (defNode->getFirstChild()->getSymbol()->isArrayShadowSymbol() ||
              !_nonColdLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(defNode->getFirstChild()->getFirstChild())))))))))
         {
         if (_valueNumberInfo->getValueNumber(defNode) != _valueNumberInfo->getValueNumber(useNode))
            {
            // If the use is outside the loop, make sure that there are stores to temp t on all possible
            // paths from the allocation to the use (load of temp t). This will ensure that a prior iteration's
            // allocation is not what is pointed at by temp t when we reach the use of temp t.
            //
            if (checkIfUseIsInSameLoopAsDef(_useDefInfo->getTreeTop(defIndex), useNode) ||
                checkIfUseIsInLoopAndOverlapping(candidate, _useDefInfo->getTreeTop(defIndex), useNode))
               {
               if (trace())
                  traceMsg(comp(), "         Def node [%p] same as candidate [%p]\n", defNode, candidate->_node);
               return false;
               }
            }
         }

      if (!seenOtherDef && defNode->getOpCode().isStore() && defNode->getSymbol()->isAutoOrParm())
         {
         if (!checkOtherDefsOfLoopAllocation(defNode->getFirstChild(), candidate, false))
            return false;
         }

      if (trace())
         traceMsg(comp(), "         Def node [%p] not the same as candidate [%p]\n", defNode, candidate->_node);
      }
   return true;
   }

bool TR_EscapeAnalysis::checkAllNewsOnRHSInLoopWithAliasing(int32_t defIndex, TR::Node *useNode, Candidate *candidate)
   {
   // _aliasesOfAllocNode contains sym refs that are just aliases for a fresh allocation
   // i.e. it is just a simple attempt at tracking allocations in cases such as :
   // ...
   // a = new A()
   // ...
   // b = a
   // ...
   // c = b
   //
   // In this case a, b and c will all be considered aliases of an alloc node and so a load of
   // any of those sym refs will be treated akin to how the fresh allocation would have been in the below logic
   //

   TR::Node *defNode = _useDefInfo->getNode(defIndex);
   int32_t useIndex = useNode->getUseDefIndex();
   bool allnewsonrhs = false;

   if ((defNode->getFirstChild() == candidate->_node) &&
       (_valueNumberInfo->getValueNumber(defNode) == _valueNumberInfo->getValueNumber(useNode)))
      {
      if (trace())
         {
         traceMsg(comp(), "      Value numbers match for def node [%p] with use node [%p]\n", defNode, useNode);
         }
      allnewsonrhs = true;
      }
   else if ((_valueNumberInfo->getValueNumber(defNode) == _valueNumberInfo->getValueNumber(candidate->_node)) &&
            (_useDefInfo->getTreeTop(defIndex)->getEnclosingBlock() == candidate->_block) &&
            _aliasesOfAllocNode->get(defNode->getSymbolReference()->getReferenceNumber()))
      {
      if (trace())
         {
         traceMsg(comp(), "      Value numbers match for def node [%p] with candidate node [%p], and def node's symref is alias of candidate allocation\n", defNode, candidate->_node);
         }
      allnewsonrhs = true;
      }
   else
      {
      allnewsonrhs = true;
      TR_UseDefInfo::BitVector defs2(comp()->allocator());
      _useDefInfo->getUseDef(defs2, useIndex);
      TR_UseDefInfo::BitVector::Cursor cursor2(defs2);

      // Loop over definitions for this use and over all the candidate
      // allocations.  If the definition comes directly from a candidate
      // for stack allocation, it's harmless; if it's copied from a
      // variable that's aliased with a candidate for stack allocation
      // that was allocated in the same block, it's harmless
      for (cursor2.SetToFirstOne(); cursor2.Valid(); cursor2.SetToNextOne())
         {
         int32_t defIndex2 = cursor2;
         if (defIndex2 == 0)
            {
            allnewsonrhs = false;
            break;
            }

         TR::Node *defNode2 = _useDefInfo->getNode(defIndex2);
         TR::Node *firstChild = defNode2->getFirstChild();

         // Is RHS for this reaching definition harmless?  I.e., not a
         // definition that can escape the loop.  It is considered harmless if
         // it is a candidate for stack allocation, an alias of a candidate
         // or it is an entry in the cache for java.lang.Integer, et al.
         bool rhsIsHarmless = false;

         for (Candidate *otherAllocNode = _candidates.getFirst(); otherAllocNode; otherAllocNode = otherAllocNode->getNext())
            {
            // A reaching definition that is an allocation node for a candidate
            // for stack allocation is harmless.  Also, a reaching definition
            // that has the value number of a candidate allocation, other than the
            // current candidate, is harmless.  The added restriction in the
            // second case avoids allowing the current candidate through from a
            // a previous loop iteration.
            if (otherAllocNode->_node == firstChild
                   || (candidate->_node != otherAllocNode->_node
                       && _valueNumberInfo->getValueNumber(otherAllocNode->_node)
                            == _valueNumberInfo->getValueNumber(firstChild)))
               {
               rhsIsHarmless = true;
               break;
               }

            if (trace())
               {
               traceMsg(comp(), "         Look at defNode2 [%p] with otherAllocNode [%p]\n", defNode2, otherAllocNode->_node);
               }

            if (!rhsIsHarmless &&
                (_valueNumberInfo->getValueNumber(defNode2) == _valueNumberInfo->getValueNumber(otherAllocNode->_node)))
               {
               TR::TreeTop *treeTop;
               bool collectAliases = false;
               _aliasesOfOtherAllocNode->empty();
               _visitedNodes->empty();
               for (treeTop = otherAllocNode->_treeTop->getEnclosingBlock()->getEntry(); treeTop; treeTop = treeTop->getNextTreeTop())
                  {
                  TR::Node *node = treeTop->getNode();
                  if (node->getOpCodeValue() == TR::BBEnd)
                     break;

                  // Until we reach otherAllocNode, call visitTree to
                  // ignore nodes in those trees.  After we've reached
                  // otherAllocNode, call collectAliasesOfAllocations to
                  // track its aliases in _aliasesOfOtherAllocNode
                  if (!collectAliases)
                     {
                     visitTree(treeTop->getNode());
                     }
                  else
                     {
                     collectAliasesOfAllocations(treeTop->getNode(), otherAllocNode->_node);
                     }

                  if (treeTop == otherAllocNode->_treeTop)
                     {
                     collectAliases = true;
                     }
                  }

               if ((_useDefInfo->getTreeTop(defIndex2)->getEnclosingBlock() == otherAllocNode->_block) &&
                   _aliasesOfOtherAllocNode->get(defNode2->getSymbolReference()->getReferenceNumber()))
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "      rhs is harmless for defNode2 [%p] with otherAllocNode [%p]\n", defNode2, otherAllocNode->_node);
                     }
                  rhsIsHarmless = true;
                  break;
                  }
               }
            }

         if (!rhsIsHarmless)
            {
            if (trace())
               {
               traceMsg(comp(), "   defNode2 vn=%d is local %d\n", _valueNumberInfo->getValueNumber(defNode2), _allLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(defNode2)));
               }

            // References to objects that were previously made local are also harmless
            if (_allLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(defNode2)))
               {
               rhsIsHarmless = true;
               }
            }

         if (!rhsIsHarmless)
            {
            // Another special case when it is certain that the rhs of the def is not a candidate allocation from a prior iteration
            // In this case we are loading a value from an Integer cache anyway and that should be an allocation that has already escaped
            // that has nothing to do with the candidate allocation
            //
            if (firstChild->getOpCode().hasSymbolReference() &&
                firstChild->getSymbol()->isArrayShadowSymbol())
               {
               TR::Node *addr = firstChild->getFirstChild();
               if (addr->getOpCode().isArrayRef())
                  {
                  TR::Node *underlyingArray = addr->getFirstChild();

                  int32_t fieldNameLen = -1;
                  char *fieldName = NULL;
                  if (underlyingArray && underlyingArray->getOpCode().hasSymbolReference() &&
                      underlyingArray->getSymbolReference()->getSymbol()->isStaticField())
                     {
                     fieldName = underlyingArray->getSymbolReference()->getOwningMethod(comp())->staticName(underlyingArray->getSymbolReference()->getCPIndex(), fieldNameLen, comp()->trMemory());
                     }

                  if (fieldName && (fieldNameLen > 10) &&
                        !strncmp("java/lang/", fieldName, 10) &&
                          (!strncmp("Integer$IntegerCache.cache", &fieldName[10], 26) ||
                           !strncmp("Long$LongCache.cache", &fieldName[10], 20) ||
                           !strncmp("Short$ShortCache.cache", &fieldName[10], 22) ||
                           !strncmp("Byte$ByteCache.cache", &fieldName[10], 20) ||
                           !strncmp("Character$CharacterCache.cache", &fieldName[10], 30)))

                     {
                     if (trace())
                        {
                        traceMsg(comp(), "         rhs is harmless for defNode2 [%p] access of Integer cache\n", defNode2);
                        }

                     rhsIsHarmless = true;
                     }
                  }
               }
            // Another special case when it is certain that the rhs of the def is not a candidate allocation from a prior iteration
            // In this case we are loading a value through a call to jitLoadFlattenableArrayElement.  Such a value must be something
            // that has already escaped and has nothing to do with the candidate allocation
            //
            else if (firstChild->getOpCode().isCall()
                     && ((firstChild->getSymbolReference()->getReferenceNumber() == TR_ldFlattenableArrayElement)
                         || comp()->getSymRefTab()->isNonHelper(firstChild->getSymbolReference(),
                                                                TR::SymbolReferenceTable::loadFlattenableArrayElementNonHelperSymbol)))
               {
               if (trace())
                  {
                  traceMsg(comp(), "         rhs is harmless for defNode2 [%p] - call to jitLoadFlattenableArrayElement\n", defNode2);
                  }
               rhsIsHarmless = true;
               }
            }

         if (!rhsIsHarmless)
            {
            if (trace())
               {
               traceMsg(comp(), "      rhs not harmless for defNode2 [%p]\n", defNode2);
               }

            allnewsonrhs = false;
            break;
            }
         }
      }

   return allnewsonrhs;
   }

bool TR_EscapeAnalysis::checkOverlappingLoopAllocation(TR::Node *useNode, Candidate *candidate)
   {
   // The allocation is inside a loop and a use has been found that has other
   // defs too. If the allocation can be used directly while the use node
   // holds a previous generation, the allocation cannot be made local.
   // To check this, walk forward from the candidate allocation to find the use
   // node. If it is found in the same extended block before the last use of the
   // allocation node the two can co-exist so the allocation cannot be local.
   //
   TR::TreeTop *treeTop;
   _visitedNodes->empty();
   _aliasesOfAllocNode->empty();
   rcount_t     numReferences = 0; //candidate->_node->getReferenceCount()-1;
   for (treeTop = candidate->_treeTop->getEnclosingBlock()->getEntry(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBEnd)
         break;
      if (!checkOverlappingLoopAllocation(treeTop->getNode(), useNode, candidate->_node, numReferences))
         return false;
     if (treeTop == candidate->_treeTop)
       numReferences = candidate->_node->getReferenceCount();

      //if (numReferences == 0)
      //   break;
      }
   return true;
   }

bool TR_EscapeAnalysis::checkOverlappingLoopAllocation(TR::Node *node, TR::Node *useNode, TR::Node *allocNode, rcount_t &numReferences)
   {
   if (_visitedNodes->get(node->getGlobalIndex()))
      {
      return true;
      }

   _visitedNodes->set(node->getGlobalIndex());

   if (node->getOpCode().isStore() && node->getSymbol()->isAutoOrParm())
      {
      if (node->getFirstChild() == allocNode)
         {
         _aliasesOfAllocNode->set(node->getSymbolReference()->getReferenceNumber());
         }
      else if (!_visitedNodes->get(node->getFirstChild()->getGlobalIndex())
                  && node->getFirstChild()->getOpCode().isLoadVarDirect()
                  && node->getFirstChild()->getSymbol()->isAutoOrParm()
                  && _aliasesOfAllocNode->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
         {
         _aliasesOfAllocNode->set(node->getSymbolReference()->getReferenceNumber());
         }
      else
         {
         _aliasesOfAllocNode->reset(node->getSymbolReference()->getReferenceNumber());
         }
      }

   if ((node != allocNode)
          && (_valueNumberInfo->getValueNumber(node) == _valueNumberInfo->getValueNumber(useNode)))
      {
      if (!(node->getOpCode().isLoadVarDirect()
            && _aliasesOfAllocNode->get(node->getSymbolReference()->getReferenceNumber()))
          && (numReferences > 0))
         {
         return false;
         }
      }
   //if (node == allocNode)
   //   {
   //   if (--numReferences == 0)
   //      return true;
   //   }
   for (int32_t i = 0; /* numReferences > 0 && */ i < node->getNumChildren(); i++)
      {
      if (!checkOverlappingLoopAllocation(node->getChild(i), useNode, allocNode, numReferences))
         return false;
      }
   return true;
   }


void TR_EscapeAnalysis::visitTree(TR::Node *node)
   {
   if (_visitedNodes->get(node->getGlobalIndex()))
      {
      return;
      }

   _visitedNodes->set(node->getGlobalIndex());

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      visitTree(node->getChild(i));
      }
   }

void TR_EscapeAnalysis::collectAliasesOfAllocations(TR::Node *node, TR::Node *allocNode)
   {
   if (_visitedNodes->get(node->getGlobalIndex()))
      {
      return;
      }

   _visitedNodes->set(node->getGlobalIndex());

   if (node->getOpCode().isStore() && node->getSymbol()->isAutoOrParm())
      {
      if (node->getFirstChild() == allocNode)
         {
         _aliasesOfOtherAllocNode->set(node->getSymbolReference()->getReferenceNumber());
         }
      else if (!_visitedNodes->get(node->getFirstChild()->getGlobalIndex())
                  && node->getFirstChild()->getOpCode().isLoadVarDirect()
                  && node->getFirstChild()->getSymbol()->isAutoOrParm()
                  && _aliasesOfOtherAllocNode->get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
         {
         _aliasesOfOtherAllocNode->set(node->getSymbolReference()->getReferenceNumber());
         }
      else
         {
         _aliasesOfOtherAllocNode->reset(node->getSymbolReference()->getReferenceNumber());
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      collectAliasesOfAllocations(node->getChild(i), allocNode);
      }
   }


bool TR_EscapeAnalysis::checkIfUseIsInSameLoopAsDef(TR::TreeTop *defTree, TR::Node *useNode)
   {
   TR::Block *block = defTree->getEnclosingBlock();
   TR_RegionStructure *highestCyclicStructure = NULL;
   TR_Structure *structure = block->getStructureOf()->getParent();
   while (structure)
      {
      if (!structure->asRegion()->isAcyclic())
         highestCyclicStructure = structure->asRegion();
      structure = structure->getParent();
      }

   if (highestCyclicStructure)
      {
      TR::NodeChecklist visited (comp());
      TR_ScratchList<TR::Block> blocksInRegion(trMemory());
      highestCyclicStructure->getBlocks(&blocksInRegion);

      ListIterator<TR::Block> blocksIt(&blocksInRegion);
      TR::Block *nextBlock;
      for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
         {
         TR::TreeTop *currentTree = nextBlock->getEntry();
         TR::TreeTop *exitTree = nextBlock->getExit();
         while (currentTree &&
                (currentTree != exitTree))
            {
            if (checkUse(currentTree->getNode(), useNode, visited))
                return true;

            currentTree = currentTree->getNextTreeTop();
            }
         }

      return false;
      }
   else
      return true;

   return true;
   }


bool TR_EscapeAnalysis::checkIfUseIsInLoopAndOverlapping(Candidate *candidate, TR::TreeTop *defTree, TR::Node *useNode)
   {
   TR::NodeChecklist visited (comp());
   TR::BlockChecklist vBlocks (comp());
   TR::TreeTop *allocTree = candidate->_treeTop;
   if (trace())
      traceMsg(comp(), "Started checking for candidate %p\n", candidate->_node);
   bool decisionMade = false;
   bool b = checkIfUseIsInLoopAndOverlapping(allocTree->getNextTreeTop(), candidate->_block->getExit(), defTree, useNode, visited, vBlocks, decisionMade);
   if (trace())
      traceMsg(comp(), "Finished checking for candidate %p\n", candidate->_node);
   return b;
   }


bool TR_EscapeAnalysis::checkIfUseIsInLoopAndOverlapping(TR::TreeTop *start, TR::TreeTop *end, TR::TreeTop *defTree, TR::Node *useNode, TR::NodeChecklist& visited, TR::BlockChecklist& vBlocks, bool & decisionMade)
   {
   TR::TreeTop *currentTree = start;
   while (currentTree && (currentTree != end))
      {
      if (checkUse(currentTree->getNode(), useNode, visited))
         {
         decisionMade = true;
         if (trace())
            traceMsg(comp(), "Returning TRUE at %p\n", currentTree->getNode());
         return true;
         }

      if (currentTree == defTree)
         {
         if (trace())
            traceMsg(comp(), "Returning FALSE at %p\n", currentTree->getNode());
         decisionMade = true;
         return false;
         }

      if (currentTree->getNode()->getOpCode().isStore() &&
          (currentTree->getNode()->getSymbolReference() == useNode->getSymbolReference()))
         {
         if (trace())
            traceMsg(comp(), "Returning FALSE at %p\n", currentTree->getNode());
         decisionMade = true;
         return false;
         }

      if ((currentTree->getNode()->getNumChildren() > 0) &&
          currentTree->getNode()->getFirstChild()->getOpCode().isStore() &&
          (currentTree->getNode()->getFirstChild()->getSymbolReference() == useNode->getSymbolReference()))
         {
         if (trace())
            traceMsg(comp(), "Returning FALSE at %p\n", currentTree->getNode());
         decisionMade = true;
         return false;
         }

      currentTree = currentTree->getNextTreeTop();
      }

   TR::Block *block = end->getEnclosingBlock();
   vBlocks.add(block);
   TR::CFG *cfg = comp()->getFlowGraph();

   for (auto nextEdge = block->getSuccessors().begin(); nextEdge != block->getSuccessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getTo());
      decisionMade = false;
      if (!vBlocks.contains(next) && (next != cfg->getEnd()))
         {
         if (trace())
            traceMsg(comp(), "Looking at block_%d\n", next->getNumber());
         bool b = checkIfUseIsInLoopAndOverlapping(next->getEntry(), next->getExit(), defTree, useNode, visited, vBlocks, decisionMade);
         if (decisionMade)
            {
            if (b)
               return true;
            }
         }
      else
         decisionMade = true;
      }

   for (auto nextEdge = block->getExceptionSuccessors().begin(); nextEdge != block->getExceptionSuccessors().end(); ++nextEdge)
      {
      TR::Block *next = toBlock((*nextEdge)->getTo());
      decisionMade = false;
      if (!vBlocks.contains(next) && (next != cfg->getEnd()))
         {
         if (trace())
            traceMsg(comp(), "Looking at block_%d\n", next->getNumber());
         bool b = checkIfUseIsInLoopAndOverlapping(next->getEntry(), next->getExit(), defTree, useNode, visited, vBlocks, decisionMade);
         if (decisionMade)
            {
            if (b)
               return true;
            }
         }
      else
         decisionMade = true;
      }

   if (trace())
      traceMsg(comp(), "Returning FALSE at block_%d\n", block->getNumber());
   return false;
   }


bool TR_EscapeAnalysis::checkUse(TR::Node *node, TR::Node *useNode, TR::NodeChecklist& visited)
   {
   if (visited.contains(node))
      return false;
   visited.add(node);

   if (node == useNode)
      return true;

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      if (checkUse(node->getChild(i), useNode, visited))
         return true;
      }

   return false;
   }


Candidate *TR_EscapeAnalysis::findCandidate(int32_t valueNumber)
   {
   for (Candidate *candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      if (candidate->_valueNumbers->element(0) == valueNumber)
         return candidate;
      }
   return NULL;
   }

bool TR_EscapeAnalysis::usesValueNumber(Candidate *candidate, int32_t valueNumber)
   {
   for (int32_t i = candidate->_valueNumbers->size()-1; i >= 0; i--)
      {
      if (candidate->_valueNumbers->element(i) == valueNumber)
         return true;
      }
   return false;
   }

// Remove any candidates that match the given value number
//
void TR_EscapeAnalysis::forceEscape(TR::Node *node, TR::Node *reason, bool forceFail)
   {
   TR::Node *resolvedNode = resolveSniffedNode(node);
   if (!resolvedNode)
      return;

   int32_t valueNumber = _valueNumberInfo->getValueNumber(resolvedNode);
   Candidate *candidate, *next;
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if (usesValueNumber(candidate, valueNumber))
         {
         if (!forceFail && checkIfEscapePointIsCold(candidate, reason))
            {
            if (!isImmutableObject(candidate))
               {
               if (trace())
                  traceMsg(comp(), "   Make [%p] contiguous because of node [%p]\n", candidate->_node, reason);
               candidate->setMustBeContiguousAllocation();
               //candidate->setLocalAllocation(false);
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "  Marking immutable candidate [%p] as referenced in forceEscape to allow for non-contiguous allocation, but compensating for escape at [%p]\n", candidate->_node, reason);
               candidate->setObjectIsReferenced();
               }
            }
         else
            {
            if(candidate->forceLocalAllocation())
               {
               if(trace())
                  traceMsg(comp(), "   Normally would fail [%p] because it escapes via node [%p] (cold %d), but user forces it to be local\n",
                          candidate->_node, reason, _inColdBlock);
               continue;
               }

            if (trace())
               traceMsg(comp(), "   Fail [%p] because it escapes via node [%p] (cold %d)\n", candidate->_node, reason, _inColdBlock);

            rememoize(candidate);
            _candidates.remove(candidate);
            }
         }
      }
   }


void TR_EscapeAnalysis::markCandidatesUsedInNonColdBlock(TR::Node *node)
   {
   TR::Node *resolvedNode = resolveSniffedNode(node);
   if (!resolvedNode)
      return;

   int32_t valueNumber = _valueNumberInfo->getValueNumber(resolvedNode);
   Candidate *candidate, *next;
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if (!candidate->usedInNonColdBlock() && usesValueNumber(candidate, valueNumber))
         {
         candidate->setUsedInNonColdBlock();
         if (trace())
            traceMsg(comp(), "   Mark [%p] used in non-cold block because of node [%p]\n", candidate->_node, node);
         }
      }
   }



bool TR_EscapeAnalysis::detectStringCopy(TR::Node *node)
   {
   TR::Node *baseNode = node->getFirstChild();
   TR::Node *valueNode = node->getSecondChild();

   Candidate *candidate, *next;
   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if ((baseNode == candidate->_node) &&
          (baseNode->getOpCodeValue() == TR::New) &&
          node->getOpCode().isIndirect() &&
          (baseNode->getFirstChild()->getSymbolReference()->getSymbol()->getStaticSymbol()->getStaticAddress() == comp()->getStringClassPointer()))
         {
         if (valueNode->getOpCode().isIndirect() &&
             (valueNode->getSymbolReference() == node->getSymbolReference()) &&
             (valueNode->getFirstChild()->getOpCode().isLoadVar() && !valueNode->getFirstChild()->getSymbolReference()->isUnresolved()) &&
             ((valueNode->getFirstChild() == candidate->_stringCopyNode) ||
              (candidate->_stringCopyNode == NULL)))
            {
            bool fitsPattern = true;
            if (!candidate->_stringCopyNode)
               {
               TR::TreeTop *cursorTree = candidate->_treeTop->getNextTreeTop();
               while (cursorTree)
                  {
                  TR::Node *cursorNode = cursorTree->getNode();
                  if ((cursorNode == node) ||
                        (cursorNode->getOpCode().isAnchor() && (cursorNode->getFirstChild() == node)) ||
                       (cursorNode->getOpCodeValue() == TR::BBEnd))
                     break;

                  if (cursorNode->getOpCodeValue() == TR::treetop)
                     {
                     if (cursorNode->getFirstChild()->getOpCode().isCall() ||
                         cursorNode->getFirstChild()->getOpCode().isStore())
                        fitsPattern = false;
                     }
                  else if (cursorNode->getOpCodeValue() == TR::astore)
                     {
                     if ((cursorNode->getFirstChild() != baseNode) &&
                         (cursorNode->getFirstChild() != valueNode->getFirstChild()))
                        fitsPattern = false;
                     }
                  else if (cursorNode->getOpCodeValue() == TR::NULLCHK)
                     {
                     if (cursorNode->getFirstChild() != valueNode)
                        fitsPattern = false;
                     }
                  else if (cursorNode->getOpCode().isAnchor())
                     {
                     // do nothing;
                     }
                  else
                     fitsPattern = false;

                  if (!fitsPattern)
                     break;

                  cursorTree = cursorTree->getNextTreeTop();
                  }
               }

            if (fitsPattern)
               {
               candidate->_stringCopyNode = valueNode->getFirstChild();
               return true;
               }
            else
               candidate->_stringCopyNode = baseNode;
            }
         else
            candidate->_stringCopyNode = baseNode;
         }
      }

   return false;
   }






// Restrict any candidates that match the given value number.
// The restriction can be to prevent the local allocation or just to make it
// a contiguous local allocation. Desynchronization is not affected.
// Return "true" if any candidates were restricted.
//
bool TR_EscapeAnalysis::restrictCandidates(TR::Node *node, TR::Node *reason, restrictionType type)
   {
   TR::Node *resolvedNode = resolveSniffedNode(node);
   if (!resolvedNode)
      return false;

   bool locked = false;
   if (reason &&
       ((reason->getOpCodeValue() == TR::monent) ||
        (reason->getOpCodeValue() == TR::monexit)))
      locked = true;

   int32_t    valueNumber = _valueNumberInfo->getValueNumber(resolvedNode);
   Candidate *candidate;
   bool       wasRestricted = false;
   for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      if (candidate->isLocalAllocation() && usesValueNumber(candidate, valueNumber))
         {
         if (reason->getOpCodeValue() == TR::arraycopy)
            candidate->_seenArrayCopy = true;

         if (locked)
            {
            if (!_inColdBlock)
               {
               candidate->setLockedInNonColdBlock(true);
               candidate->setUsedInNonColdBlock(true);
               if (trace())
                  traceMsg(comp(), "   Mark [%p] used and locked in non-cold block because of node [%p]\n", candidate->_node, node);
               }

            candidate->setLockedObject(true);
            int32_t lockedObjectValueNumber = _valueNumberInfo->getValueNumber(reason->getFirstChild());
            Candidate *lockedCandidate = findCandidate(lockedObjectValueNumber);
            if (!lockedCandidate)
               {
               if (trace())
                  traceMsg(comp(), "   Make [%p] non-local because of node [%p]\n", candidate->_node, reason);
               wasRestricted = true;
               //candidate->setLocalAllocation(false);
               forceEscape(reason->getFirstChild(), reason);
               continue;
               }

            TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
            if (_parms && fej9->hasTwoWordObjectHeader())
               {
               TR_ScratchList<TR_ResolvedMethod> resolvedMethodsInClass(trMemory());
               fej9->getResolvedMethods(trMemory(), (TR_OpaqueClassBlock *) candidate->_class, &resolvedMethodsInClass);
               bool containsSyncMethod = false;
               ListIterator<TR_ResolvedMethod> resolvedIt(&resolvedMethodsInClass);
               TR_ResolvedMethod *resolvedMethod;
               for (resolvedMethod = resolvedIt.getFirst(); resolvedMethod; resolvedMethod = resolvedIt.getNext())
                  {
                  if (resolvedMethod->isSynchronized())
                     {
                     containsSyncMethod = true;
                     break;
                     }
                  }
               if (!containsSyncMethod)
                  {
                  if (trace())
                     traceMsg(comp(), "   Make [%p] non-local because of node [%p]\n", candidate->_node, reason);
                  wasRestricted = true;
                  candidate->setLocalAllocation(false);
                  continue;
                  }
               }
            }

         if (type == MakeNonLocal)
            {
            if (checkIfEscapePointIsCold(candidate, reason))
               {
                 //candidate->setObjectIsReferenced();
               if (trace())
                  traceMsg(comp(), "   Do not make [%p] non-local because of cold node [%p]\n", candidate->_node, reason);
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "   Make [%p] non-local because of node [%p]\n", candidate->_node, reason);
               candidate->setLocalAllocation(false);
               }

            if (!isImmutableObject(candidate))
               wasRestricted = true;
            }
         else
            {
            if (type == MakeContiguous)
               {
               // make contiguous
               //
              if (checkIfEscapePointIsCold(candidate, reason))
                 {
                   //candidate->setObjectIsReferenced();
                 if (trace())
                     traceMsg(comp(), "   Do not make [%p] contiguous because of cold node [%p]\n", candidate->_node, reason);
                 }
              else
                 {
                 if (trace())
                     traceMsg(comp(), "   Make [%p] contiguous because of node [%p]\n", candidate->_node, reason);
                 candidate->setMustBeContiguousAllocation();
                 }

               if (!isImmutableObject(candidate))
                  wasRestricted = true;
               }
            else if (!candidate->objectIsReferenced() &&
                     !candidate->mustBeContiguousAllocation())
               {
               if (trace())
                  traceMsg(comp(), "   Make [%p] object-referenced because of node [%p]\n", candidate->_node, reason);
               candidate->setObjectIsReferenced();
               wasRestricted = true;
               }
            }
         }
      }
   return wasRestricted;
   }




bool TR_EscapeAnalysis::checkIfEscapePointIsCold(Candidate *candidate, TR::Node *node)
   {
   if (_curBlock->isOSRCodeBlock() ||
       _curBlock->isOSRCatchBlock())
      return false;

   if (isEscapePointCold(candidate, node))
      {
      int32_t j;
      bool canStoreToHeap = true;
      for(j=0;j<node->getNumChildren();j++)
         {
         TR::Node *child = node->getChild(j);
         TR::Node *resolvedChildAtTopLevel = resolveSniffedNode(child);
         if (!resolvedChildAtTopLevel)
            continue;

         if (usesValueNumber(candidate, _valueNumberInfo->getValueNumber(resolvedChildAtTopLevel)))
            {
            if (resolvedChildAtTopLevel->getOpCode().isLoadVarDirect() &&
                (_curBlock != candidate->_block) &&
                (_curBlock != comp()->getStartBlock()))
               {
               bool recognizedCatch = true;
               if (_curBlock->isCatchBlock())
                  {
                  TR::Node *firstTree = _curBlock->getEntry()->getNextTreeTop()->getNode();
                  if (!firstTree->getOpCode().isStoreDirect() ||
                      !firstTree->getSymbol()->isAuto() ||
                      !firstTree->getFirstChild()->getOpCode().hasSymbolReference() ||
                      (firstTree->getFirstChild()->getSymbolReference() != comp()->getSymRefTab()->findOrCreateExcpSymbolRef()))
                     recognizedCatch = false;
                  }
               if (recognizedCatch)
                  {
                  if (trace())
                     traceMsg(comp(), "Adding cold block info for child %p value number %d candidate %p\n", child, _valueNumberInfo->getValueNumber(resolvedChildAtTopLevel), candidate->_node);

                  candidate->addColdBlockEscapeInfo(_curBlock, resolvedChildAtTopLevel, _curTree);
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "   For candidate [%p], seen an unexpected opcode in child [%p] of call [%p]\n", candidate->_node, child, node);

                  canStoreToHeap = false;
                  }
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "   For candidate [%p], seen an unexpected opcode in child [%p] of call [%p]\n", candidate->_node, child, node);

               canStoreToHeap = false;
               }
            }
         }

      if (canStoreToHeap)
         {
         candidate->setObjectIsReferenced();
         if (trace())
            traceMsg(comp(), "  Marking candidate [%p] as referenced in checkIfEscapePointIsCold - escape point [%p]\n", candidate->_node, node);

         if (!isImmutableObject(candidate) && (_parms || !node->getOpCode().isReturn()))
            {
            //candidate->setObjectIsReferenced();
            if (trace())
               traceMsg(comp(), "   Make candidate [%p] contiguous to allow heapification\n", candidate->_node);
            candidate->setMustBeContiguousAllocation();
            }

         return true;
         }
      }
   return false;
   }




static void checkForDifferentSymRefs(Candidate *candidate, int32_t i, TR::SymbolReference *symRef, TR_EscapeAnalysis *ea, bool peeking)
   {
   static char *dontCheckForDifferentSymRefsInEA = feGetEnv("TR_dontCheckForDifferentSymRefsInEA");
   if (dontCheckForDifferentSymRefsInEA)
      {
      // We don't need this logic anymore generally.  Leaving it in place for
      // Java8 release to reduce disruption and risk.
      //
      return;
      }

   TR::Compilation *comp = TR::comp();
   TR::SymbolReference *memorizedSymRef = candidate->_fields->element(i).fieldSymRef();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (memorizedSymRef &&
       (memorizedSymRef != symRef) &&
       (symRef->isUnresolved() ||
        memorizedSymRef->isUnresolved() ||
        ((symRef->getOffset() >= (int32_t) fej9->getObjectHeaderSizeInBytes()) &&
         (memorizedSymRef->getOffset() >= (int32_t) fej9->getObjectHeaderSizeInBytes()))))
      {
      if (  memorizedSymRef->getCPIndex() == -1
         || symRef->getCPIndex() == -1
         || !TR::Compiler->cls.jitFieldsAreSame(comp, memorizedSymRef->getOwningMethod(comp), memorizedSymRef->getCPIndex(), symRef->getOwningMethod(comp), symRef->getCPIndex(), symRef->getSymbol()->isStatic()))
         {
         bool aliasingIsSafe = false;
         if (peeking)
            {
            // Aliasing queries don't even work on peeked nodes because their
            // symRef numbers refer to a different symRef table, so aliasing
            // info can't possibly be required for correctness.
            //
            // Having said that, we'll use "false" here to be conservative.
            //
            aliasingIsSafe = false;
            }
         else
            {
            aliasingIsSafe = symRef->getUseDefAliases(false).contains(memorizedSymRef, comp);
            }
         if (!aliasingIsSafe)
            {
            if (ea->trace())
               {
               traceMsg(comp, "candidate n%dn %p excluded coz of ambiguous field symrefs #%d [%s] and [%s]\n",
                  candidate->_node->getGlobalIndex(), candidate->_node,
                  memorizedSymRef->getReferenceNumber(), memorizedSymRef->getName(comp->getDebug()),
                  symRef->getName(comp->getDebug()));
               }
            candidate->setLocalAllocation(false);
            }
         }
      }
   }


TR::SymbolReference *FieldInfo::fieldSymRef()
   {
   return _goodFieldSymrefs->getHeadData();
   }

bool FieldInfo::symRefIsForFieldInAllocatedClass(TR::SymbolReference *symRef)
   {
   if (_goodFieldSymrefs->find(symRef))
      return true;

   if (_badFieldSymrefs->find(symRef))
      return false;

   TR_ASSERT(0, "symRefIsForFieldInAllocatedClass expects symref #%d to have been remembered", symRef->getReferenceNumber());
   return true;
   }

bool FieldInfo::hasBadFieldSymRef()
   {
   return !_badFieldSymrefs->isEmpty();
   }

void FieldInfo::rememberFieldSymRef(TR::Node *fieldNode, Candidate *candidate, TR_EscapeAnalysis *ea)
   {
   TR_ASSERT(!ea->_parms, "rememberFieldSymRef: cannot remember peeked field SymRefs");

   TR::SymbolReference *symRef = fieldNode->getSymbolReference();
   if (!_goodFieldSymrefs->find(symRef) && !_badFieldSymrefs->find(symRef))
      {
      bool isGood = false;
      switch (candidateHasField(candidate, fieldNode, _offset, ea))
         {
         case TR_yes:
            isGood = true;
            break;
         case TR_no:
            isGood = false;
            break;
         default:
            // Older (questionable) r11-era logic based on object size bound
            isGood = (_offset + _size <= candidate->_size);
            break;
         }

      if (isGood)
         {
         int32_t fieldSize = fieldNode->getSize();
         if (ea->comp()->useCompressedPointers() && fieldNode->getDataType() == TR::Address)
            fieldSize = TR::Compiler->om.sizeofReferenceField();
         _size = fieldSize;
         _goodFieldSymrefs->add(symRef);
         }
      else
         {
         _badFieldSymrefs->add(symRef);
         }
      }
   }

void FieldInfo::rememberFieldSymRef(TR::SymbolReference *symRef, TR_EscapeAnalysis *ea)
   {
   TR_ASSERT(!ea->_parms, "rememberFieldSymRef: cannot remember peeked field SymRefs");

   if (!_goodFieldSymrefs->find(symRef) && !_badFieldSymrefs->find(symRef))
      {
      int32_t fieldSize = symRef->getSymbol()->getSize();
      if (ea->comp()->useCompressedPointers() && symRef->getSymbol()->getDataType() == TR::Address)
         fieldSize = TR::Compiler->om.sizeofReferenceField();

      _size = fieldSize;
      _goodFieldSymrefs->add(symRef);
      }
   }


// Remember the use of a field in any candidates that can match the given node.
//
void TR_EscapeAnalysis::referencedField(TR::Node *base, TR::Node *field, bool isStore, bool seenSelfStore, bool seenStoreToLocalObject)
   {
   TR::Node *resolvedNode = resolveSniffedNode(base);
   if (!resolvedNode)
      return;

   TR::SymbolReference *symRef = field->getSymbolReference();
   if (symRef->isUnresolved())
      {
      forceEscape(base, field, true);
      return;
      }

   bool usesStackTrace = false;
   if (!isStore)
      {
      if (symRef->getSymbol()->getRecognizedField() == TR::Symbol::Java_lang_Throwable_stackTrace)
         usesStackTrace = true;
      }
   int32_t    valueNumber = _valueNumberInfo->getValueNumber(resolvedNode);
   int32_t    storedValueNumber = -1;
   if (seenStoreToLocalObject)
      {
      TR::Node *resolvedStoredValueNode = resolveSniffedNode(field->getSecondChild());
      if (resolvedStoredValueNode)
         storedValueNumber = _valueNumberInfo->getValueNumber(resolvedStoredValueNode);
      else
         seenStoreToLocalObject = false;
      }

   Candidate *candidate;
   for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      if (seenStoreToLocalObject)
         {
         if (candidate->isLocalAllocation() && usesValueNumber(candidate, storedValueNumber))
            {
            if (candidate->isInsideALoop())
               {
               candidate->setLocalAllocation(false);
               if (trace())
                  traceMsg(comp(), "7 setting local alloc %p to false\n", candidate->_node);
               }
            else
               candidate->_seenStoreToLocalObject = true;
            }
         }

      if (candidate->isLocalAllocation() && usesValueNumber(candidate, valueNumber))
         {
         if (usesStackTrace)
            {
            candidate->setUsesStackTrace();
            candidate->setMustBeContiguousAllocation();
            if (trace())
               traceMsg(comp(), "   Make [%p] contiguous because of setUsesStackTrace\n", candidate->_node);
            }

         // Only remember fields that are actually present in the allocated
         // object. It is possible to reach uses which (via downcasting) are
         // referencing a derived class, or even an unrelated class.  At run
         // time the local allocation would never reach these program points,
         // but they are still "reachable" from a dataflow perspective, so we
         // must be aware of them.
         //
         if (isStore)
            {
            candidate->_seenFieldStore = true;
            if (seenSelfStore)
               candidate->_seenSelfStore = true;
            }

         int32_t fieldOffset = symRef->getOffset();
         if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
            {
            fieldOffset = symRef->getOffset();
            }
         else
            {
            TR::Node *offsetNode = NULL;
            if (field->getFirstChild()->getOpCode().isArrayRef())
               offsetNode = field->getFirstChild()->getSecondChild();

            if (offsetNode && offsetNode->getOpCode().isLoadConst())
               {
               if (offsetNode->getType().isInt64())
                  fieldOffset = (int32_t) offsetNode->getLongInt();
               else
                  fieldOffset = offsetNode->getInt();
               }
            }

         candidate->findOrSetFieldInfo(field, symRef, fieldOffset, field->getSize(), field->getDataType(), this);
         }
      }
   }


// Resolve the node if it is a parameter reference
//
TR::Node *TR_EscapeAnalysis::resolveSniffedNode(TR::Node *node)
   {
   if (_parms == NULL)
      return node;
   if (!node->getOpCode().isLoadVarOrStore() &&
       (node->getOpCodeValue() != TR::loadaddr))
      return NULL;
   TR::Symbol *sym = node->getSymbol();
   if (!sym->isParm())
      return NULL;
   return _parms->element(sym->getParmSymbol()->getOrdinal());
   }

void TR_EscapeAnalysis::checkEscape(TR::TreeTop *firstTree, bool isCold, bool & ignoreRecursion)
   {
   TR::Node    *node;
   TR::TreeTop *treeTop;

   // Do string copy pattern matching first and fix value numbers accordingly
   for (treeTop = firstTree; treeTop && !_candidates.isEmpty(); treeTop = treeTop->getNextTreeTop())
      {
      node = treeTop->getNode();
      if (node->getOpCode().isStoreIndirect() && detectStringCopy(node))
         {
         TR::Node *baseNode = node->getFirstChild();
         int32_t baseNodeVN = _valueNumberInfo->getValueNumber(baseNode);
         TR::Node *copyNode = node->getSecondChild()->getFirstChild();
         int32_t copyNodeVN = _valueNumberInfo->getValueNumber(copyNode);
         Candidate *baseCandidate = findCandidate(baseNodeVN);
         Candidate *candidate = NULL;

         TR_ASSERT(baseCandidate, "There must be a candidate corresponding to the base node VN");
         if (trace())
            traceMsg(comp(), "Base candidate: [%p], base node VN: %d, copy node VN: %d\n",
                    baseCandidate->_node, baseNodeVN, copyNodeVN);

         for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
            {
            if (usesValueNumber(candidate, copyNodeVN))
               {
               for (int32_t i = baseCandidate->_valueNumbers->size()-1; i >= 0; i--)
                  {
                  int32_t valueNumber = baseCandidate->_valueNumbers->element(i);
                  if (!candidate->_valueNumbers->contains(valueNumber))
                     candidate->_valueNumbers->add(valueNumber);
                  }
               }
            }
         }
      else if (0 && (node->getOpCodeValue() != TR::call) &&
               (node->getNumChildren() > 0))
         {
         node = node->getFirstChild();
         if ((node->getOpCodeValue() == TR::call) &&
             !node->getSymbolReference()->isUnresolved() &&
             !node->getSymbol()->castToMethodSymbol()->isHelper() &&
             (node->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getRecognizedMethod() == TR::java_lang_String_init_String) &&
             (treeTop->getPrevTreeTop()->getNode()->getNumChildren() > 0) &&
             (node->getFirstChild()->getOpCodeValue() == TR::New) &&
             (node->getFirstChild() == treeTop->getPrevTreeTop()->getNode()->getFirstChild()))
            {
            TR::Node *baseNode = node->getFirstChild();
            Candidate *candidate, *next;
            for (candidate = _candidates.getFirst(); candidate; candidate = next)
               {
               next = candidate->getNext();

               if ((candidate->_stringCopyNode == NULL) ||
                   (candidate->_stringCopyNode == node->getSecondChild()))
                  {
                  if ((baseNode == candidate->_node) &&
                      (baseNode->getOpCodeValue() == TR::New) &&
                      (baseNode->getFirstChild()->getSymbolReference()->getSymbol()->getStaticSymbol()->getStaticAddress() == comp()->getStringClassPointer()) &&
                      (candidate->_treeTop->getNextTreeTop() == treeTop))
                     {
                     candidate->_stringCopyCallTree = treeTop;
                     candidate->_stringCopyNode = node->getSecondChild();
                     //traceMsg(comp(), "11cand node %p string copy node %p and node %p\n", candidate->_node, candidate->_stringCopyNode, node);
                     TR::Node *baseNode = node->getFirstChild();
                     int32_t baseNodeVN = _valueNumberInfo->getValueNumber(baseNode);
                     TR::Node *copyNode = node->getSecondChild();
                     int32_t copyNodeVN = _valueNumberInfo->getValueNumber(copyNode);
                     Candidate *baseCandidate = findCandidate(baseNodeVN);
                     Candidate *candidate = NULL;

                     TR_ASSERT(baseCandidate, "There must be a candidate corresponding to the base node VN");
                     if (trace())
                        traceMsg(comp(), "Base candidate: [%p], base node VN: %d, copy node VN: %d\n",
                              baseCandidate->_node, baseNodeVN, copyNodeVN);

                     for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
                        {
                        if (usesValueNumber(candidate, copyNodeVN))
                           {
                           for (int32_t i = baseCandidate->_valueNumbers->size()-1; i >= 0; i--)
                              {
                              int32_t valueNumber = baseCandidate->_valueNumbers->element(i);
                              if (!candidate->_valueNumbers->contains(valueNumber))
                                 candidate->_valueNumbers->add(valueNumber);
                              }
                           }
                        }
                     }
                  else if (candidate->_node == node->getFirstChild())
                     {
                     candidate->_stringCopyNode = node->getFirstChild();
                     //traceMsg(comp(), "22cand node %p string copy node %p and node %p\n", candidate->_node, candidate->_stringCopyNode, node);
                     }
                  }
               }
            }
         }
      }

   // First eliminate all allocations that are not allowed because of nodes
   // other than calls, then go through again and eliminate the allocations
   // that are not allowed because of calls alone. This reduces the amount of
   // sniffing required into called methods.
   //
   _classObjectLoadForVirtualCall = false;
   TR::NodeChecklist vnsNoCall (comp());

   for (treeTop = firstTree; treeTop && !_candidates.isEmpty(); treeTop = treeTop->getNextTreeTop())
      {
      node = treeTop->getNode();
      if (!_parms)
         {
         _curTree = treeTop;
         //_curNode = node;
         }

      if (node->getOpCodeValue() == TR::BBStart)
         {
         if (!_parms || !_inColdBlock)
            {
            _inColdBlock = false;
            if (!_parms)
               _curBlock = node->getBlock();
            if (((_curBlock->isCold() ||
                  _curBlock->isCatchBlock() ||
                  //(_curBlock->getHotness(comp()->getFlowGraph()) == deadCold)) &&
                  (_curBlock->getFrequency() == (MAX_COLD_BLOCK_COUNT+1))) &&
                 !_parms) ||
                isCold)
               _inColdBlock = true;
            }
         }

      if (!vnsNoCall.contains(node))
         checkEscapeViaNonCall(node, vnsNoCall);
      }

   bool oldIgnoreRecursion = ignoreRecursion;
   TR::NodeChecklist vnsCall (comp());
   for (treeTop = firstTree; treeTop && !_candidates.isEmpty(); treeTop = treeTop->getNextTreeTop())
      {
      node = treeTop->getNode();
      if (!_parms)
         {
         _curTree = treeTop;
         //_curNode = node;
         }

     if (node->getOpCodeValue() == TR::BBStart)
         {
         if (!_parms || !_inColdBlock)
            {
            _inColdBlock = false;
            if (!_parms)
                _curBlock = node->getBlock();
            if ((_curBlock->isCold() ||
                 _curBlock->isCatchBlock() ||
                 //(_curBlock->getHotness(comp()->getFlowGraph()) == deadCold)) &&
                (_curBlock->getFrequency() == (MAX_COLD_BLOCK_COUNT+1))) &&
                !_parms)
                _inColdBlock = true;
            }
         }

      ignoreRecursion = oldIgnoreRecursion;
      if (node->getOpCode().isCheck() || node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();
      if (node->getOpCode().isCall() && !vnsCall.contains(node))
         {
         if (node->getSymbolReference()->getReferenceNumber() != TR_prepareForOSR
             || comp()->getOSRMode() != TR::voluntaryOSR
             ||  _curBlock->isOSRInduceBlock())
            checkEscapeViaCall(node, vnsCall, ignoreRecursion);
         }
      }
   }

static bool isConstantClass(TR::Node *classNode, TR_EscapeAnalysis *ea)
   {
   bool result = false;
   TR::Compilation *comp = ea->comp();

   // New logic.
   // If classNode represents a class address, return that class.
   //
   if (  classNode->getOpCodeValue() == TR::loadaddr
      && classNode->getSymbol()->isStatic()
      && !classNode->getSymbolReference()->isUnresolved()
      ){
      result = true;
      }

   if (ea->trace())
      traceMsg(comp, "   isConstantClass(%p)=%s (supportsInliningOfIsInstance=%s)\n", classNode, result?"true":"false", comp->cg()->supportsInliningOfIsInstance()?"true":"false");
   return result;
   }

static bool isFinalizableInlineTest(TR::Compilation *comp, TR::Node *candidate, TR::Node *root, TR::Node *vftLoad)
   {
   TR_ASSERT(vftLoad->getOpCode().isLoadIndirect() &&
             vftLoad->getFirstChild() == candidate &&
             vftLoad->getSymbolReference()->getSymbol()->isClassObject() &&
             candidate->getOpCode().isRef(),
             "Expecting indirect load of class ptr off potential candidate reference");

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool is64Bit = comp->target().is64Bit();

   // root
   //   r1      - first child of root
   //     r11   - first child of first child of root
   //     r12   - second child of first child of root
   //   r2      - second child of root
   TR::Node *r1 = (root->getNumChildren() > 0) ? root->getFirstChild() : NULL;
   TR::Node *r2 = (root->getNumChildren() > 1) ? root->getSecondChild() : NULL;
   TR::Node *r11 = (r1 && r1->getNumChildren() > 0) ? r1->getFirstChild() : NULL;
   TR::Node *r12 = (r1 && r1->getNumChildren() > 1) ? r1->getSecondChild() : NULL;

   bool castDownToInt = is64Bit && r11 && (r11->getOpCodeValue() == TR::l2i);
   bool usesLongOps = is64Bit && !castDownToInt;

   TR::ILOpCodes ifOp = usesLongOps ? TR::iflcmpne : TR::ificmpne;
   TR::ILOpCodes andOp = usesLongOps ? TR::land : TR::iand;
   TR::ILOpCodes loadOp = is64Bit ? TR::lloadi : TR::iloadi;
   TR::ILOpCodes constOp = usesLongOps ? TR::lconst : TR::iconst;

   TR::Node *loadNode = castDownToInt ? r11->getFirstChild() : r11;

   /*
      Looking for a pattern of:

      if[i/l]cmpne                                      <root>
         [i/l]and                                       <r1>
            [i/l]loadi <classDepthAndFlags>             <r11/loadNode>
               aloadi <vft-symbol>                      <vftLoad>
                  ...                                   <ref>
            [i/l]const <FlagValueForFinalizerCheck>     <r12>
         [i/l]const 0                                   <r2>

       or

      ificmpne                                          <root>
         iand                                           <r1>
            l2i                                         <r11>
               lloadi <classDepthAndFlags>              <loadNode>
                  aloadi <vft-symbol>                   <vftLoad>
                     ...                                <ref>
            iconst <FlagValueForFinalizerCheck>         <r12>
         iconst 0                                       <r2>
   */

   return root->getOpCodeValue() == ifOp &&
          r1->getOpCodeValue() == andOp &&
          r2->getOpCodeValue() == constOp &&
          (usesLongOps ? r2->getLongInt() : r2->getInt() ) == 0 &&
          loadNode->getOpCodeValue() == loadOp &&
          r12->getOpCodeValue() == constOp &&
          (usesLongOps ? r12->getLongInt() : r12->getInt())
                == fej9->getFlagValueForFinalizerCheck() &&
          loadNode->getFirstChild() == vftLoad /*&& (implied by the above assume)
          root->getFirstChild()->getFirstChild()->getFirstChild()->getFirstChild() == candidate*/;
   }

void TR_EscapeAnalysis::checkEscapeViaNonCall(TR::Node *node, TR::NodeChecklist& visited)
   {
   visited.add(node);

   int32_t    i;
   int32_t    valueNumber;
   Candidate *candidate;
   bool       wasRestricted = false;
   TR::Node   *child;

   // Handle cases that can validly perform an array calculation via TR::aiadd and
   // that don't force the base candidate to be contiguous.
   // Other uses of TR::aiadd to address fields or access array elements will
   // be found during normal processing of checkEscapeViaNonCall and force the
   // base candidate to be contiguous.
   //
   if (node->getOpCode().isLoadVarOrStore() &&
       node->getSymbol()->isArrayShadowSymbol() &&
       node->getOpCode().isIndirect())
      {
      child = node->getFirstChild();
      if (child->getOpCode().isArrayRef())
         {
         TR::Node *arrayOffset = child->getSecondChild();
         if (arrayOffset->getOpCodeValue() == TR::iconst || arrayOffset->getOpCodeValue() == TR::lconst)
            {
            valueNumber = _valueNumberInfo->getValueNumber(child->getFirstChild());
            if (findCandidate(valueNumber))
               visited.add(child);
            }
         }
      }

   if (node->getOpCode().isBooleanCompare() && node->getFirstChild()->getType().isAddress())
      {
      // If child is a candidate, prohibit dememoization
      //
      if (node->getSecondChild()->isNull())
         {
         // dememoization doesn't break compares with NULL
         }
      else
         {
         for (i = node->getNumChildren() - 1; i >= 0; i--)
            {
            child = node->getChild(i);
            candidate = findCandidate(_valueNumberInfo->getValueNumber(child));
            if (candidate && candidate->_dememoizedConstructorCall)
               {
               if (trace())
                  traceMsg(comp(), "Rememoize [%p] due to [%p] under address compare [%p]\n", candidate->_node, child, node);
               rememoize(candidate);
               _candidates.remove(candidate);
               }
            }
         }
      }

   bool classObjectLoadForVirtualCall = _classObjectLoadForVirtualCall;
   for (i = node->getNumChildren() - 1; i >= 0; i--)
      {
      child = node->getChild(i);
      if (!visited.contains(child))
         {
         if (node->getOpCode().isCallIndirect() &&
             (i < node->getFirstArgumentIndex()))
            _classObjectLoadForVirtualCall = true;
         else
            _classObjectLoadForVirtualCall = false;

         checkEscapeViaNonCall(child, visited);

         _classObjectLoadForVirtualCall = false;
         }
      }

   if (node->getOpCodeValue() == TR::areturn)
      {
      static const char *noChar = feGetEnv("TR_NoChar");

      if (_parms)
         {
         // In a called method, if we're returning an argument to the caller,
         // we can just add the call's value number to the bag of value numbers
         // for the candidate corresponding to the argument being returned.
         // If there is some reason to worry (eg. an assignment to the parm),
         // that will be rejected by other means.
         //
         // HOWEVER this logic has been disabled because at this stage it is
         // too late to add new value numbers to candidates.  Much analysis has
         // already occurred, and won't be repeated for the newly added VNs.
         //
         TR::Node *argument = resolveSniffedNode(node->getFirstChild());
         TR::Node *callNode = _curTree->getNode()->getFirstChild();

         bool doit = true;
          if (!_methodSymbol ||
              !_inBigDecimalAdd ||
             (_methodSymbol->getRecognizedMethod() != TR::java_math_BigDecimal_possibleClone))
              doit = false;
          else
             {
             if (trace())
                traceMsg(comp(), "seen bd possible clone at node %p\n", callNode);
             }

         if (  argument
            && doit
            && (callNode->getOpCodeValue() == TR::acall || callNode->getOpCodeValue() == TR::acalli)
            && _sniffDepth == 1 // Note: this only goes one level deep.  If a returns it to b which returns it to the caller, we'll call that an escape.
            && performTransformation(comp(), "%sPeeked method call [%p] returning argument [%p] NOT considered an escape\n", OPT_DETAILS, callNode, argument))
            {
            // Bingo.  Add the call node's value number to the bag for every
            // candidate for which argument is already in the bag.
            //
            if (trace())
               traceMsg(comp(), "call node is %p (depth %d) for which return opt was done\n", callNode, _sniffDepth);
#if 0
            //
            //Note : It's too late anyway for this here...see above comment...so it is disabled
            //
            int32_t argumentValueNumber = _valueNumberInfo->getValueNumber(argument);
            int32_t callValueNumber     = _valueNumberInfo->getValueNumber(callNode);

            if (trace())
               traceMsg(comp(), "   Adding call VN %d to all candidates that already have arg VN %d\n", callValueNumber, argumentValueNumber);

            for (Candidate *candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
               {
               if (candidate->_valueNumbers->contains(argumentValueNumber))
                  {
                  if (trace())
                     traceMsg(comp(), "     %s\n", comp()->getDebug()->getName(candidate->_node));
                  candidate->_valueNumbers->add(callValueNumber);
                  }
               }
#endif
            }
         else
            {
            forceEscape(node->getFirstChild(), node, true);
            return;
            }
         }

      // If a return is the only way this object escapes, we cannot make
      // it a local allocation but we can desynchronize it.
      //
      if (!_methodSymbol /* ||
          noChar ||
          ( comp()->getJittedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_toString) ||
          (_methodSymbol->getRecognizedMethod() != TR::java_math_BigDecimal_longString1) */)
         restrictCandidates(node->getFirstChild(), node, MakeNonLocal);
      return;
      }

   if (node->getOpCodeValue() == TR::athrow)
      {
      forceEscape(node->getFirstChild(), node);
      return;
      }

   if (node->getOpCodeValue() == TR::newvalue)
      {
      // Except for the class address - child operand zero - the
      // operands of a TR::newvalue should be considered to escape
      // through that operation.
      //
      for (int i = 1; i < node->getNumChildren(); i++)
         {
         forceEscape(node->getChild(i), node);
         }
      return;
      }

   if (node->getOpCode().hasSymbolReference() &&
         (node->getSymbolReference()->getSymbol() == comp()->getSymRefTab()->findGenericIntShadowSymbol() ||
          node->getSymbol()->isUnsafeShadowSymbol() ||
          node->getDataType().isBCD() ||
          node->getSymbolReference()->getSymbol()->getDataType().isBCD()))
      {
      // PR 80372: Don't take any chances with low-level symbols like GIS and
      // Unsafe.  Reject the base object as a candidate, and continue processing
      // in case node is a store and we need to force the RHS to escape.
      //
      TR::Node *baseRef = node->getFirstChild();
      while (baseRef->getOpCode().isArrayRef())
         baseRef = baseRef->getFirstChild();

      forceEscape(baseRef, node, true);
      }

   if (node->getOpCode().isArrayRef())
      {
      // If the base of the address calculation is a candidate, it must be
      // contiguous
      //
      restrictCandidates(node->getFirstChild(), node, MakeContiguous);
      return;
      }

   // Handle loads and stores
   //
   if (node->getOpCode().isLoadVarOrStore())
      {
      if (!_inColdBlock)
         markCandidatesUsedInNonColdBlock(node);

      if (node->getOpCode().isIndirect())
         {
         bool seenSelfStore = false;
         bool seenStoreToLocalObject = false;

         // Handle escapes via indirect store of a candidate
         //
         if (node->getOpCode().isStore())
            {
            TR::Node *baseObject = node->getFirstChild();
            if (node->getSymbol()->isArrayShadowSymbol() &&
                baseObject->getOpCode().isArrayRef())
              baseObject = baseObject->getFirstChild();

            //detectStringCopy(node);

            // TODO : the code related to local object value numbers
            // can be turned back on if value numbering gives same value numbers
            // to 2 loads of fields of local objects. This is currently not done
            // in value numbering. As a result,
            // <local_object>.f = <new_that_may_be_stack_allocated>
            // <static> = <local_object>.f
            // is a problematic case. We won't track the above escape point and
            // wrongly stack allocate the new. If we are able to track a local
            // object field's value numbers we can hope to see if it 'really' escapes
            // or not.
            //
            //if (!baseObject->getOpCode().hasSymbolReference() ||
            //        (!(_nonColdlocalObjectsValueNumbers &&
            //        _nonColdlocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(baseObject)))))

            // java/lang/Throwable.cause addition in SC1411, which disables fillInStackTrace opt in javac
            if ((node->getSecondChild() != baseObject) &&
                (!baseObject->getOpCode().hasSymbolReference() ||
                 !node->getSecondChild()->getOpCode().hasSymbolReference() ||
                 ((baseObject->getReferenceCount() != 1) &&
                  (!((baseObject->getReferenceCount() == 2) && (node->getOpCodeValue() == TR::awrtbari) && (node->getChild(2) == baseObject)))) ||
                 (node->getSecondChild()->getReferenceCount() != 1) ||
                 (baseObject->getSymbolReference() != node->getSecondChild()->getSymbolReference())))
               {
               TR::Node *resolvedBaseObject = resolveSniffedNode(baseObject);
               bool stringCopyOwningMethod = false;
               TR::ResolvedMethodSymbol *owningMeth = node->getSymbolReference()->getOwningMethodSymbol(comp());
               TR::RecognizedMethod methodId = owningMeth->getRecognizedMethod();
               if (methodId == TR::java_lang_String_init_String)
                  {
                  char *sig = owningMeth->getResolvedMethod()->signatureChars();
                  if (!strncmp(sig, "(Ljava/lang/String;)", 20))
                     {
                     stringCopyOwningMethod = true;
                     }
                  }

               if ((!_nonColdLocalObjectsValueNumbers ||
                    !_notOptimizableLocalObjectsValueNumbers ||
                    !resolvedBaseObject ||
                    (comp()->useCompressedPointers() && (TR::Compiler->om.compressedReferenceShift() > 3) && !cg()->supportsStackAllocations()) ||
                    !resolvedBaseObject->getOpCode().hasSymbolReference() ||
                    !_nonColdLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(resolvedBaseObject)) ||
                    (((node->getSymbolReference()->getSymbol()->getRecognizedField() != TR::Symbol::Java_lang_String_value) ||
                       stringCopyOwningMethod ||
                      _notOptimizableLocalStringObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(resolvedBaseObject))) &&
                     _notOptimizableLocalObjectsValueNumbers->get(_valueNumberInfo->getValueNumber(resolvedBaseObject)))))
                  {
                  forceEscape(node->getSecondChild(), node);
                  }
               else
                  {
                  seenStoreToLocalObject = true;
                  restrictCandidates(node->getSecondChild(), node, MakeContiguous);
                  }
               }
            else
               {
               static char *allowSelfStoresToLocalObjects = feGetEnv("TR_allowSelfStoresToLocalObjects");
               if (allowSelfStoresToLocalObjects)
                  {
                  seenSelfStore = true;
                  if (baseObject->getOpCode().hasSymbolReference())
                     restrictCandidates(node->getSecondChild(), node, MakeContiguous);
                  }
               else
                  {
                  forceEscape(node->getSecondChild(), node);
                  }
               }
            }

         child = node->getFirstChild();
         if (node->getSymbol()->isArrayShadowSymbol() &&
             child->getOpCode().isArrayRef())
            child = child->getFirstChild();

         // Remember indirect loads or stores for fields of local allocations.
         //
         if (node->getOpCode().isStore())
            {
            //if (seenSelfStore)
            referencedField(child, node, true, seenSelfStore, seenStoreToLocalObject);
            //else
            //   referencedField(child, node, true);
            }
         else
            referencedField(child, node, false);

         // If the field is unresolved it must be left as a field reference, so
         // force the allocation to be contiguous.
         //
         if (node->getSymbolReference()->isUnresolved())
            restrictCandidates(child, node, MakeContiguous);
         // Similarly if the field is not a regular field make the allocation
         // contiguous (unless it's a class load for a vcall or inline finalizable test)
         // (If it's an inline finalizable test we have to clean the test up since the object will no longer exist)
         //
         else if (node->getSymbolReference()->getOffset() < (int32_t)comp()->fej9()->getObjectHeaderSizeInBytes() && !node->getSymbol()->isArrayShadowSymbol())
            {
            if (!node->getSymbolReference()->getSymbol()->isClassObject() || node->getOpCode().isStore() ||
                (!classObjectLoadForVirtualCall && !isFinalizableInlineTest(comp(), child, _curTree->getNode(), node)))
               {
               restrictCandidates(child, node, MakeContiguous);
               }
            }
         }

      else // Direct load or store
         {
         if (node->getOpCode().isStore() &&
             (comp()->getSymRefTab()->findThisRangeExtensionSymRef() != node->getSymbolReference()))
            {
            // Handle escapes via direct store of a candidate.
            // In a sniffed method even a store into a local may escape since
            // we can't keep track of the local stored into (we have no value
            // number information for it).
            //
            if ((_parms || !node->getSymbol()->isAutoOrParm()) &&
                (!_methodSymbol ||
                 (_parms && node->getSymbol()->isAutoOrParm() && !node->getFirstChild()->isThisPointer()) ||
                ((_methodSymbol->getRecognizedMethod() != TR::java_math_BigDecimal_add) &&
                 (_methodSymbol->getRecognizedMethod() != TR::java_math_BigDecimal_longAdd) &&
                 (_methodSymbol->getRecognizedMethod() != TR::java_math_BigDecimal_slAdd))))
               forceEscape(node->getFirstChild(), node);

            // PR 93460
            if (node->getSymbolReference()->getSymbol()->isAuto() &&
                node->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
                restrictCandidates(node->getFirstChild(), node, MakeContiguous);

            // Handle escapes via store into a parameter for a called method
            // (this is conservative, but hopefully it doesn't happen too often)
            //
            if (_parms)
               forceEscape(node, node);
            }
         }
      return;
      }

   // Handle arraycopy nodes.
   // Any candidates being used for arraycopy must be allocated contiguously.
   //
   if (node->getOpCodeValue() == TR::arraycopy)
      {
      if (node->getNumChildren() == 5)
         {
         restrictCandidates(node->getFirstChild(), node, MakeContiguous);
         restrictCandidates(node->getSecondChild(), node, MakeContiguous);
         }

      if (node->getNumChildren() == 3)
         {
         if (node->getFirstChild()->getOpCode().isArrayRef())
            restrictCandidates(node->getFirstChild()->getFirstChild(), node, MakeContiguous);
         if (node->getSecondChild()->getOpCode().isArrayRef())
            restrictCandidates(node->getSecondChild()->getFirstChild(), node, MakeContiguous);
         }

      return;
      }

   if (_sniffDepth > 0)
      return;

   // Look for some direct references to candidates, and decide if a separate
   // object must be created for them to handle the references.
   //
   if (node->getOpCode().isCheckCast() ||
       node->getOpCodeValue() == TR::instanceof)
      {
      valueNumber = _valueNumberInfo->getValueNumber(node->getFirstChild());
      candidate = findCandidate(valueNumber);
      if (candidate)
         {
         TR::Node *classNode  = node->getSecondChild();
         if (!isConstantClass(classNode, this))
            {
            // We don't know the class statically. The object must stay contiguous.
            //
            wasRestricted |= restrictCandidates(node->getFirstChild(), node, MakeContiguous);
            }
         else if (node->getOpCode().isCheckCast() &&
                  comp()->fej9()->isInstanceOf((TR_OpaqueClassBlock *) candidate->_class, (TR_OpaqueClassBlock*)classNode->getSymbol()->castToStaticSymbol()->getStaticAddress(), true) != TR_yes)
            {
            // We will be able to predict the result of the cast. For instanceof and
            // for a succeeding checkcast we will remove this candidate reference.
            // For a failing checkcast the candidate reference will remain, but can
            // become a separate "java/lang/Object" object.
            //
            wasRestricted |= restrictCandidates(node->getFirstChild(), node, MakeObjectReferenced);
            }
         }
      }

   else if (node->getOpCodeValue() == TR::ifacmpeq ||
             node->getOpCodeValue() == TR::ifacmpne)
      {
      // If one of the children is a candidate we may be able to determine if
      // the comparison will succeed or fail
      //
      int32_t firstValue  = _valueNumberInfo->getValueNumber(node->getFirstChild());
      int32_t secondValue = _valueNumberInfo->getValueNumber(node->getSecondChild());
      bool    predictResult = false;
      if (firstValue == secondValue)
         predictResult = true;
      else
         {
         candidate = findCandidate(firstValue);
         if (candidate && !usesValueNumber(candidate, secondValue))
            predictResult = true;
         else
            {
            candidate = findCandidate(secondValue);
            if (candidate && !usesValueNumber(candidate, firstValue))
               predictResult = true;
            }
         }

      if (!predictResult)
         {
         wasRestricted = restrictCandidates(node->getFirstChild(), node, MakeObjectReferenced);
         wasRestricted |= restrictCandidates(node->getSecondChild(), node, MakeObjectReferenced);
         }
      }

   else if (node->getOpCodeValue() != TR::treetop &&
            !node->getOpCode().isArrayRef() &&
            node->getOpCodeValue() != TR::PassThrough &&
            !node->getOpCode().isArrayLength() &&
            node->getOpCodeValue() != TR::NULLCHK &&
            node->getOpCodeValue() != TR::ResolveCHK &&
            node->getOpCodeValue() != TR::ResolveAndNULLCHK &&
            node->getOpCodeValue() != TR::instanceof &&
#if CHECK_MONITORS
            ((node->getOpCodeValue() != TR::monent && node->getOpCodeValue() != TR::monexit) || !_removeMonitors) &&
#endif
            !node->getOpCode().isCall())
      {
      // Any other direct reference to a candidate means that there must be a
      // local object even if the fields are allocated non-contiguously
      //
      for (i = node->getNumChildren()-1; i >= 0; i--)
         {
         child = node->getChild(i);
         wasRestricted |= restrictCandidates(child, node, MakeObjectReferenced);
         }
      }
   if (wasRestricted)
      {
      if (trace())
         {
         /////printf("secs Object referenced via %s in %s\n", node->getOpCode().getName(), comp()->signature());
         traceMsg(comp(), "Object referenced via %s\n", node->getOpCode().getName());
         }
      }
   }


bool
SniffCallCache::isInCache(TR_LinkHead<SniffCallCache> *sniffCacheList, TR_ResolvedMethod *vmMethod, bool isCold, int32_t &bytecodeSize)
   {
   SniffCallCache *sniffCallCache;
   for (sniffCallCache = sniffCacheList->getFirst(); sniffCallCache; sniffCallCache = sniffCallCache->getNext())
      {
      if (vmMethod->isSameMethod(sniffCallCache->_vmMethod) &&
          isCold == sniffCallCache->_isCold)
         {
         bytecodeSize = sniffCallCache->_bytecodeSize;
         return true;
         }
      }
   return false;
   }

TR::SymbolReference*
SymRefCache::findSymRef(TR_LinkHead<SymRefCache> *symRefList, TR_ResolvedMethod *resolvedMethod)
   {
   SymRefCache *symRefCache;
   for (symRefCache = symRefList->getFirst(); symRefCache; symRefCache = symRefCache->getNext())
      {
       if (resolvedMethod->isSameMethod(symRefCache->getMethod()))
         {
          return symRefCache->getSymRef();
         }
      }
   return NULL;
   }

void TR_EscapeAnalysis::checkEscapeViaCall(TR::Node *node, TR::NodeChecklist& visited, bool & ignoreRecursion)
   {
   int32_t  nodeVN;
   TR::Node *value;

   visited.add(node);

   TR::ResolvedMethodSymbol *methodSymbol = node->getSymbol()->getResolvedMethodSymbol();
   Candidate               *candidate, *next;
   bool                     needToSniff = false;

   // In Java9 sun_misc_Unsafe_putInt might be the wrapper which could potentially be redefined,
   // so add a check for isNative to be conservative.
   if (methodSymbol && _methodSymbol &&
       (_methodSymbol->getRecognizedMethod() == TR::java_math_BigDecimal_longString1C) &&
       (methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putInt_jlObjectII_V) &&
       (methodSymbol->isNative()))
      {
      if (trace())
         traceMsg(comp(), "Ignoring escapes via call [%p] \n", node);
      return;
      }

   for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      if (  methodSymbol
         && methodSymbol->getRecognizedMethod() == TR::java_lang_Integer_init
         && candidate->_dememoizedConstructorCall
         && candidate->_dememoizedConstructorCall->getNode()->getFirstChild() == node)
         {
         if (trace())
            traceMsg(comp(), "Ignoring escapes via call [%p] that will either be inlined or rememoized\n", node);
         return;
         }
      }

   for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      candidate->setArgToCall(_sniffDepth, false);
      candidate->setNonThisArgToCall(_sniffDepth, false);
      }

   int32_t thisVN = -1;
   int32_t firstArgIndex = node->getFirstArgumentIndex();
   for (int32_t arg = firstArgIndex; arg < node->getNumChildren() && !_candidates.isEmpty(); arg++)
      {
      checkEscapeViaNonCall(node->getChild(arg), visited);
      value = resolveSniffedNode(node->getChild(arg));
      if (!value)
         continue;

      // Method arguments are always escapes when method enter/exit hooks are enabled
      // because the agent can ask for method receivers and return values, and inspect them.
      // be conservative in checking for method exit hook: if an argument is returned by the callee exit hook would make it escape
      //
      if (TR::Compiler->vm.canMethodEnterEventBeHooked(comp()) || TR::Compiler->vm.canMethodExitEventBeHooked(comp()))
         {
         forceEscape(node->getChild(arg), node, true);
         continue;
         }

      nodeVN = _valueNumberInfo->getValueNumber(value);
      if (arg == firstArgIndex)
         thisVN = nodeVN;

      for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
         {
         if (usesValueNumber(candidate, nodeVN))
            {
            // Remember calls to fillInStackTrace and printStackTrace. These
            // are methods of Throwable. The call to fillInStackTrace is very
            // expensive, and if it can be proved that the stack trace is not
            // used (via a call to printStackTrace) then the calls to
            // fillInStackTrace can be removed.
            //
            if (arg == firstArgIndex && methodSymbol && (!node->getOpCode().isIndirect() || !methodSymbol->getResolvedMethod()->virtualMethodIsOverridden()))
               {
               TR::RecognizedMethod methodId = methodSymbol->getRecognizedMethod();
               if (methodId == TR::java_lang_Throwable_fillInStackTrace)
                  {
                  // No escape via this method, and don't make this call the
                  // reason for making the object contiguous.
                  //
                  if (candidate->_valueNumbers->element(0) == nodeVN)
                     candidate->setFillsInStackTrace();
                  continue;
                  }

               if (methodId == TR::java_lang_Throwable_printStackTrace)
                  {
                  // Uses stack trace, so fillInStackTrace can't be avoided.
                  // Force the object to be contiguous and sniff this method to
                  // check for escape.
                  //
                  candidate->setUsesStackTrace();
                  candidate->setMustBeContiguousAllocation();
                  if (trace())
                     traceMsg(comp(), "   Make [%p] contiguous because of setUsesStackTrace\n", candidate->_node);
                  }
               }

            // Ignore calls to jitCheckIfFinalizeObject, the argument cannot escape via such calls
            //
            if (1)
               {
               TR::SymbolReference *finalizeSymRef = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitCheckIfFinalizeObject, true, true, true);
               if (node->getSymbolReference() == finalizeSymRef)
                  {
                  if (trace())
                     traceMsg(comp(), "Candidate [%p] cannot escape because of node [%p]\n", candidate->_node, node);
                  continue;
                  }
               }


            // We can't sniff JNI methods.  Check if this is a JNI call, in which
            // case use the table of trusted methods to see if the argument can escape from
            // this call
            //
            if (!node->getOpCode().isIndirect() && methodSymbol && methodSymbol->getResolvedMethod()->isJNINative())
               {
               if (methodSymbol && !comp()->fej9()->argumentCanEscapeMethodCall(methodSymbol, arg-firstArgIndex))
                  {
                  // The candidate will have to be kept as a contiguous
                  // allocation, since it is passed to another method.
                  //
                  candidate->setMustBeContiguousAllocation();
                  if (trace())
                     traceMsg(comp(), "   Make [%p] contiguous because of node [%p]\n", candidate->_node, node);
                  continue;
                  }
               }

            // Otherwise we need to sniff the method. If we can't sniff it for
            // some reason then assume that this candidate can escape.
            //
            needToSniff = true;

            // Remember that this child is an affected candidate
            //
            candidate->setArgToCall(_sniffDepth);
            if (node->getOpCode().isIndirect())
               {
               if (arg != firstArgIndex)
                  candidate->setNonThisArgToCall(_sniffDepth);
               }

            if (arg == (firstArgIndex+1) && methodSymbol && !node->getOpCode().isIndirect())
               {
               TR::RecognizedMethod methodId = methodSymbol->getRecognizedMethod();
               if (methodId == TR::java_lang_String_init_String)
                  {
                  char *sig = methodSymbol->getResolvedMethod()->signatureChars();
                  if (!strncmp(sig, "(Ljava/lang/String;)", 20))
                     {
                     candidate->setCallsStringCopyConstructor();
                     }
                  }
               }
            }
         }
      }

   if (needToSniff)
      {
      if (methodSymbol)
         dumpOptDetails(comp(), "Sniff call %p called %s\n", node, methodSymbol->getMethod()->signature(trMemory()));
      TR_LinkHead<SniffCallCache> sniffCacheList;
      sniffCacheList.setFirst(NULL);
      bool sniffCallInCache;

      bool oldIgnoreRecursion = ignoreRecursion;
      int32_t bytecodeSize = sniffCall(node, methodSymbol, false, _inColdBlock, ignoreRecursion);
      int32_t originalBytecodeSize = bytecodeSize;
      for (candidate = _candidates.getFirst(); candidate; candidate = next)
         {
         bool refinedSniff = false;
         next = candidate->getNext();
         bytecodeSize = originalBytecodeSize;
         TR::SymbolReference *symRef = node->getSymbolReference();
         if (candidate->isArgToCall(_sniffDepth))
            {
            if (!candidate->isNonThisArgToCall(_sniffDepth) &&
                node->getOpCode().isIndirect() &&
                ((candidate->_node->getOpCodeValue() == TR::New)
                 || (candidate->_node->getOpCodeValue() == TR::newvalue)) &&
                (thisVN > -1) &&
                usesValueNumber(candidate, thisVN))
               {
               TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp());
               TR_ResolvedMethod *resolvedMethod = NULL;
               TR::MethodSymbol *sym = symRef->getSymbol()->castToMethodSymbol();
               TR::Method * originalMethod = sym->getMethod();
               int32_t len = originalMethod->classNameLength();
               char *s = TR::Compiler->cls.classNameToSignature(originalMethod->classNameChars(), len, comp());
               TR_OpaqueClassBlock *originalMethodClass = comp()->fej9()->getClassFromSignature(s, len, owningMethod);
               TR_OpaqueClassBlock *thisType = (TR_OpaqueClassBlock *) candidate->_node->getFirstChild()->getSymbol()->castToStaticSymbol()->getStaticAddress();
               int32_t offset = -1;

               bool resolvedMethodMustExist = false;
               if (originalMethodClass &&
                   thisType)
                  {
                  if (comp()->fej9()->isInstanceOf(thisType, originalMethodClass, true) == TR_yes)
                     resolvedMethodMustExist = true;
                  }

               // The 2nd check below is to account for the rare case when we can
               // have a virtual (non interface) call that has an interface method
               // as the callee in the IL. This can happen in cases when an abstract class
               // implements an interface but doesn't even declare one of the methods in the
               // interface. In this case, the vft slot for the abstract class actually
               // has the interface method and this can lead to strange behaviour when we look
               // up the method (code guarded by below if). The strange case is if
               // there is some other class implementing the interface unrelated to the abstract class
               // and we try and look up the method in that class (because its an instance of
               // the interface) and get a completely unrelated method.
               //
               if (resolvedMethodMustExist &&
                   (sym->isInterface() || !TR::Compiler->cls.isInterfaceClass(comp(), originalMethodClass)))
                  {
                  if (sym->isInterface() &&
                      originalMethodClass)
                     {
                     int32_t cpIndex = symRef->getCPIndex();
                     resolvedMethod = owningMethod->getResolvedInterfaceMethod(comp(), thisType, cpIndex);
                     if (resolvedMethod)
                        offset = owningMethod->getResolvedInterfaceMethodOffset(thisType, cpIndex);
                     }
                  else if (sym->isVirtual()
                     && !symRef->isUnresolved()
                     && sym->getRecognizedMethod() != TR::java_lang_Object_newInstancePrototype) // 166813: Temporary fix
                     {
                     offset = symRef->getOffset();
                     resolvedMethod = owningMethod->getResolvedVirtualMethod(comp(), thisType, offset);
                     }
                  }

               if (resolvedMethod)
                  {
                 // try to use cached result (_curTree, bytecodeSize) before sniffing into the call
                  sniffCallInCache = SniffCallCache::isInCache(&sniffCacheList, resolvedMethod, _inColdBlock, bytecodeSize);
                  dumpOptDetails(comp(), "Refined sniff call %p called %s, cached=%s\n",
                                   node, resolvedMethod->signature(trMemory()), sniffCallInCache ? "true" : "false");
                  if (!sniffCallInCache)
                     {
                     TR::SymbolReference * newSymRef;
                     // we want to reuse the IL if we're sniffing the same method
                     newSymRef = SymRefCache::findSymRef(&(getOptData()->_symRefList), resolvedMethod);
                     if (!newSymRef)
                        {
                        newSymRef = getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::Virtual);
                        SymRefCache *symRefCache = new (trHeapMemory()) SymRefCache(newSymRef, resolvedMethod);
                        getOptData()->_symRefList.add(symRefCache);
                        newSymRef->copyAliasSets(symRef, getSymRefTab());
                        newSymRef->setOffset(offset);
                        }
                     TR::MethodSymbol *newMethodSymbol = newSymRef->getSymbol()->castToMethodSymbol();

                     ignoreRecursion = oldIgnoreRecursion;
                     bytecodeSize = sniffCall(node, newMethodSymbol->getResolvedMethodSymbol(), true, _inColdBlock, ignoreRecursion);
                     SniffCallCache *sniffCallCache = new (trStackMemory()) SniffCallCache(newMethodSymbol->getResolvedMethodSymbol()->getResolvedMethod(), _inColdBlock, bytecodeSize);
                     sniffCacheList.add(sniffCallCache);
                     }
                  refinedSniff = true;
                  //printf("bytecodeSize = %d original bytecodeSize = %d method %s\n", bytecodeSize, originalBytecodeSize, resolvedMethod->signature());
                  }
               }

            if ((bytecodeSize > 0) && (!node->getOpCode().isIndirect() || !node->isTheVirtualCallNodeForAGuardedInlinedCall()) &&
                (!symRef->isUnresolved() ||
                 !refinedSniff) )
               {
               // The sniff was successful.
               // If this is the top-level call site, remember that it
               // references the candidate. If we cannot inline the call site
               // then the candidate will have to be a contiguous allocation
               // in order for it to be passed to the callee.
               //
               candidate->setArgToCall(_sniffDepth, false);
               candidate->setNonThisArgToCall(_sniffDepth, false);

               if (!symRef->isUnresolved() ||
                   !refinedSniff)
                  {
                  // The sniff was successful.
                  // If this is the top-level call site, remember that it
                  // references the candidate. If we cannot inline the call site
                  // then the candidate will have to be a contiguous allocation
                  // in order for it to be passed to the callee.
                  //

                  if (_sniffDepth == 0)
                     {
                     candidate->addCallSite(_curTree);
                     if (!refinedSniff && node->getOpCode().isIndirect() && (methodSymbol && !methodSymbol->getResolvedMethod()->virtualMethodIsOverridden()))
                        candidate->addVirtualCallSiteToBeFixed(_curTree);
                     }

                  if (_sniffDepth > candidate->_maxInlineDepth)
                     candidate->_maxInlineDepth = _sniffDepth;
                  candidate->_inlineBytecodeSize += bytecodeSize;

                  }
               else
                  candidate->setMustBeContiguousAllocation();

               if (trace())
                  traceMsg(comp(), "   Make [%p] contiguous because of call node [%p]\n", candidate->_node, node);
               }
            else
               {
               // If the escape point is cold, this will not
               // prevent us from stack allocating it. We will compensate
               // for this later
               //
               if (checkIfEscapePointIsCold(candidate, node))
                  continue;

               // Force
               if(candidate->forceLocalAllocation())
                  {
                  if (trace())
                     traceMsg(comp(), "   Normally [%p] would fail because child of call [%p] to %s, but user wants it locally allocated\n",
                          candidate->_node, node,
                          node->getSymbol()->getMethodSymbol()->getMethod()
                             ? node->getSymbol()->getMethodSymbol()->getMethod()->signature(trMemory())
                             : "[Unknown method]");
                  continue;
                  }
               // The sniff could not be done. Remove this candidate.
               //

              if (trace())
                  traceMsg(comp(), "   Fail [%p] because child of call [%p]\n",
                          candidate->_node, node);

               rememoize(candidate);
               _candidates.remove(candidate);
               }
            }
         //else
         //  traceMsg(comp(), "NOT is arg to call at node %p\n", node);
         }
      }
   }

int32_t TR_EscapeAnalysis::sniffCall(TR::Node *callNode, TR::ResolvedMethodSymbol *methodSymbol, bool ignoreOpCode, bool isCold, bool & ignoreRecursion)
   {
   if (_sniffDepth >= _maxSniffDepth)
      return 0;
   if (!methodSymbol)
      return 0;
   if (!ignoreOpCode &&
       (callNode->getOpCode().isIndirect() &&
       (methodSymbol->getResolvedMethod()->virtualMethodIsOverridden() || isCold || (_sniffDepth !=0) || (manager()->numPassesCompleted() == _maxPassNumber))))
      return 0;

   // Avoid attempting to devirtualize a computed call
   if (methodSymbol->isComputed())
      return 0;

   TR_ResolvedMethod *method = methodSymbol->getResolvedMethod();
   if (!method)
      return 0;

   if (!method->isCompilable(trMemory()) || method->isJNINative())
      return 0;

    uint32_t bytecodeSize = method->maxBytecodeIndex();
    if (bytecodeSize > MAX_SNIFF_BYTECODE_SIZE)
      return 0;

    getOptData()->_totalPeekedBytecodeSize += bytecodeSize;
    if (getOptData()->_totalPeekedBytecodeSize > _maxPeekedBytecodeSize)
       return 0;

   TR::ResolvedMethodSymbol *owningMethodSymbol = callNode->getSymbolReference()->getOwningMethodSymbol(comp());
   TR_ResolvedMethod* owningMethod = owningMethodSymbol->getResolvedMethod();
   TR_ResolvedMethod* calleeMethod = methodSymbol->getResolvedMethod();

   if (owningMethod->isSameMethod(calleeMethod) &&
       (comp()->getJittedMethodSymbol() != owningMethodSymbol))
      {
      if (ignoreRecursion)
         return bytecodeSize;
      else
         ignoreRecursion = true;
      }

   if (trace())
      traceMsg(comp(), "\nDepth %d sniffing into call at [%p] to %s\n", _sniffDepth, callNode, method->signature(trMemory()));

   vcount_t visitCount = comp()->getVisitCount();
   if (!methodSymbol->getFirstTreeTop())
      {
      //comp()->setVisitCount(1);

      dumpOptDetails(comp(), "O^O ESCAPE ANALYSIS: Peeking into the IL to check for escaping objects \n");

      bool ilgenFailed = false;
      bool isPeekableCall = ((PersistentData *)manager()->getOptData())->_peekableCalls->get(callNode->getGlobalIndex());
      if (isPeekableCall)
         ilgenFailed = (NULL == methodSymbol->getResolvedMethod()->genMethodILForPeekingEvenUnderMethodRedefinition(methodSymbol, comp()));
      else
         ilgenFailed = (NULL == methodSymbol->getResolvedMethod()->genMethodILForPeeking(methodSymbol, comp()));
      //comp()->setVisitCount(visitCount);

      /*
       * Under HCR we cannot generally peek methods because the method could be
       * redefined and any assumptions we made about the contents of the method
       * would no longer hold. Peeking is only safe when we add a compensation
       * path that would handle potential escape of candidates if a method were
       * redefined.
       *
       * Under OSR we know the live locals from the OSR book keeping information
       * so can construct a helper call which appears to make those calls escape.
       * Such a call will force heapification of any candidates ahead of the
       * helper call.
       *
       * We, therefore, queue this call node to be protected if we are in a mode
       * where protection is possible (voluntary OSR, HCR on). The actual code
       * transformation is delayed to the end of EA since it will disrupt value
       * numbering. The transform will insert an HCR guard with an escape helper
       * call as follows:
       *
       * |   ...  |      |      ...      |
       * | call m |  =>  | hcr guard (m) |
       * |   ...  |      +---------------+
       *                         |        \
       *                         |         +-(cold)-------------------+
       *                         |         | call eaEscapeHelper      |
       *                         |         |   <loads of live locals> |
       *                         |         +--------------------------+
       *                         V        /
       *                 +---------------+
       *                 | call m        |
       *                 |      ...      |
       *
       * Once EA finishes the postEscapeAnalysis pass will clean-up all
       * eaEscapeHelper calls. Code will only remain in the taken side of the
       * guard if candidtes were stack allocated and required heapficiation.
       *
       * This code transformation is protected with a perform transformation at
       * the site of tree manipulation at the end of this pass of EA.
       * Protecting any call is considered a reason to enable another pass of
       * EA if we have not yet reached the enable limit.
       *
       * Note that the result of the call can continue to participate in
       * commoning with the above design. If we duplicated the call on both
       * sides of the guard the result could not participate in commoning.
       *
       */
      if (ilgenFailed)
         {
         if (trace())
            traceMsg(comp(), "   (IL generation failed)\n");
         static char *disableHCRCallPeeking = feGetEnv("TR_disableEAHCRCallPeeking");
         if (!isPeekableCall
             && disableHCRCallPeeking == NULL
             && comp()->getOSRMode() == TR::voluntaryOSR
             && comp()->getHCRMode() == TR::osr
             && !_candidates.isEmpty()
             && !((TR_EscapeAnalysis::PersistentData*)manager()->getOptData())->_processedCalls->get(callNode->getGlobalIndex()))
            {
            dumpOptDetails(comp(), "%sAdding call [%p] n%dn to list of calls to protect for peeking to increase opportunities for stack allocation\n", OPT_DETAILS, callNode, callNode->getGlobalIndex());
            TR_BitVector *candidateNodes = new (comp()->trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
            TR_BitVector *symRefsToLoad = new (comp()->trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);

            for (int32_t arg = callNode->getFirstArgumentIndex(); arg < callNode->getNumChildren(); ++arg)
               {
               TR::Node *child = callNode->getChild(arg);
               int32_t valueNumber = _valueNumberInfo->getValueNumber(child);
               for (Candidate *candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
                  {
                  if (usesValueNumber(candidate, valueNumber))
                     {
                     candidateNodes->set(candidate->_node->getGlobalIndex());
                     ListIterator<TR::SymbolReference> itr(candidate->getSymRefs());
                     for (TR::SymbolReference *symRef = itr.getFirst(); symRef; symRef = itr.getNext())
                        {
                        symRefsToLoad->set(symRef->getReferenceNumber());
                        }
                     }
                  }
               }

            (*_callsToProtect)[callNode] = std::make_pair(candidateNodes, symRefsToLoad);
            }
         return 0;
         }

      if (trace())
         {
    //comp()->setVisitCount(1);
         for (TR::TreeTop *tt = methodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
            getDebug()->print(comp()->getOutFile(), tt);
         //comp()->setVisitCount(visitCount);
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "   (trees already dumped)\n");
      }

   int32_t firstArgIndex = callNode->getFirstArgumentIndex();
   TR_Array<TR::Node*> *newParms =  new (trStackMemory()) TR_Array<TR::Node*>(trMemory(), callNode->getNumChildren() - firstArgIndex, false, stackAlloc);
   for (int32_t i = firstArgIndex; i < callNode->getNumChildren(); ++i)
      {
      newParms->add(resolveSniffedNode(callNode->getChild(i)));
      }

   TR_Array<TR::Node*> *oldParms = _parms;
   _parms = newParms;
   TR::TreeTop *curTree = _curTree;
   bool inColdBlock = _inColdBlock;
   //TR::Node *curNode = _curNode;
   ++_sniffDepth;

   TR::ResolvedMethodSymbol *oldMethodSymbol = _methodSymbol;

   bool oldInBigDecimalAdd = _inBigDecimalAdd;
   if (_methodSymbol && (_methodSymbol->getRecognizedMethod() == TR::java_math_BigDecimal_add))
      _inBigDecimalAdd = true;
   else
      _inBigDecimalAdd = false;

   _methodSymbol = methodSymbol;

   checkEscape(methodSymbol->getFirstTreeTop(), isCold, ignoreRecursion);

   _methodSymbol = oldMethodSymbol;
   _inBigDecimalAdd = oldInBigDecimalAdd;

   _curTree = curTree;
   ///// FIXME : This line should be uncommented
   ////// It is checked in commented out only temporarily
   //////
   /////
   _inColdBlock = inColdBlock;
   _parms = oldParms;
   --_sniffDepth;

#if CHECK_MONITORS
   /* monitors */
   if (_removeMonitors)
      {
      TR_MonitorStructureChecker inspector;
      if (inspector.checkMonitorStructure(methodSymbol->getFlowGraph()))
         {
         _removeMonitors = false;
         if (trace())
            traceMsg(comp(), "Disallowing monitor-removal because of strange monitor structure in sniffed method %s\n",
                        methodSymbol->getMethod()->signature(trMemory()));
         }
      }
#endif

   return bytecodeSize;
   }

// Check for size limits, both on individual object sizes and on total
// object allocation size.
// FIXME: need to modify this method to give priority to non-array objects
//
void TR_EscapeAnalysis::checkObjectSizes()
   {
   int32_t totalSize = 0;
   int32_t i;
   Candidate *candidate, *next;

   for (candidate = _candidates.getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if (!candidate->isLocalAllocation())
         continue;

      // Make sure contiguous objects are not too big
      //
      if (candidate->isContiguousAllocation())
         {
         if (candidate->_size > MAX_SIZE_FOR_ONE_CONTIGUOUS_OBJECT)
            {
            if (trace())
               /////printf("secs   Fail [%p] because object size is too big in %s\n", candidate->_node, comp()->signature());
               ;
            if (trace())
               traceMsg(comp(), "   Fail [%p] because object size %d is too big\n", candidate->_node, candidate->_size);

            candidate->setLocalAllocation(false);
            }
         else
            totalSize += candidate->_size;
         }
      else
         {
         if (candidate->_fields)
            {
            // Size of non-contiguous object is the sum of its referenced fields
            //
            for (i = candidate->_fields->size()-1; i >= 0; i--)
               candidate->_fieldSize += candidate->_fields->element(i)._size;
            totalSize += candidate->_fieldSize;
            }
         }
      }

   // If total size for local allocation is too big, remove candidates until the
   // total size becomes reasonable.
   //
   while (totalSize > MAX_SIZE_FOR_ALL_OBJECTS)
      {
      // Remove the largest contiguous allocation. If there is none, remove
      // the largest non-contiguous allocation.
      //
      int32_t    largestContiguousObjectSize    = -1;
      Candidate *largestContiguousObject        = NULL;
      int32_t    largestNonContiguousObjectSize = -1;
      Candidate *largestNonContiguousObject     = NULL;

      for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
         {
         if (!candidate->isLocalAllocation())
            continue;

         if (candidate->isContiguousAllocation())
            {
            if (candidate->_size > largestContiguousObjectSize)
               {
               largestContiguousObjectSize = candidate->_size;
               largestContiguousObject = candidate;
               }
            }
         else
            {
            if (candidate->_fieldSize > largestNonContiguousObjectSize)
               {
               largestNonContiguousObjectSize = candidate->_fieldSize;
               largestNonContiguousObject = candidate;
               }
            }
         }
      if (largestContiguousObjectSize > 0)
         {
         candidate = largestContiguousObject;
         totalSize -= largestContiguousObjectSize;
         }
      else
         {
         candidate = largestNonContiguousObject;
         totalSize -= largestNonContiguousObjectSize;
         }

      if (trace())
         {
         /////printf("secs   Fail [%p] because total object size is too big in %s\n", candidate->_node, comp()->signature());
         traceMsg(comp(), "   Fail [%p] because total object size is too big\n", candidate->_node);
         }

      // Mark the candidate as not available for local allocation
      //
      candidate->setLocalAllocation(false);
      }
   }

void TR_EscapeAnalysis::anchorCandidateReference(Candidate *candidate, TR::Node *reference)
   {
   // If this candidate reference (which is about to be removed) may be
   // referenced later in the block, insert an anchoring treetop for it after
   // the current treetop.
   //
   if (reference->getReferenceCount() > 1 &&
       _curTree->getNextTreeTop()->getNode()->getOpCodeValue() != TR::BBEnd &&
       (candidate->isContiguousAllocation() || candidate->objectIsReferenced()))
      {
      // Anchor the child
      //
      TR::TreeTop::create(comp(), _curTree, TR::Node::create(TR::treetop, 1, reference));
      }
   }

void TR_EscapeAnalysis::fixupTrees()
   {
   TR::NodeChecklist visited (comp());
   TR::TreeTop *treeTop, *nextTree;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = nextTree)
      {
      nextTree = treeTop->getNextTreeTop();
      _curTree = treeTop;
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         _curBlock = node->getBlock();
      else if (!visited.contains(node))
         {
         if (fixupNode(node, NULL, visited))
            {
            dumpOptDetails(comp(), "%sRemoving tree rooted at [%p]\n",OPT_DETAILS, node);
            _somethingChanged=true;
            TR::TransformUtil::removeTree(comp(), treeTop);
            }
         }
      }
   }

static bool shouldRemoveTree(Candidate *candidate, TR::Block *block)
   {
   ListIterator<TR_ColdBlockEscapeInfo> coldBlkInfoIt(candidate->getColdBlockEscapeInfo());
   TR_ColdBlockEscapeInfo *info;
   for (info = coldBlkInfoIt.getFirst(); info != NULL; info = coldBlkInfoIt.getNext())
      {
      TR::Block *coldBlk = info->getBlock();
      if (block == coldBlk)
         return false;
      }

   return true;
   }


// Fix up the node if it is a reference to a field of a local allocation.
// Return true if the node is to be removed.
//
bool TR_EscapeAnalysis::fixupNode(TR::Node *node, TR::Node *parent, TR::NodeChecklist& visited)
   {
   int32_t    i;
   int32_t    valueNumber;
   Candidate *candidate;
   TR::Node   *child;

   visited.add(node);

   bool                     removeThisNode = false;
   TR::ResolvedMethodSymbol *calledMethod = NULL;

   // Look for indirect loads or stores for fields of local allocations.
   //
   if (node->getOpCode().isIndirect() &&
       node->getOpCode().isLoadVarOrStore() &&
       !comp()->suppressAllocationInlining())
      {
      if (node->getSymbol()->isArrayShadowSymbol() &&
          node->getFirstChild()->getOpCode().isArrayRef())
         child = node->getFirstChild()->getFirstChild();
      else
         child = node->getFirstChild();

      valueNumber = _valueNumberInfo->getValueNumber(child);
      for (candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
         {
         if (comp()->generateArraylets()
             && (candidate->_kind != TR::New)
             && (candidate->_kind != TR::newvalue))
            continue;

         bool usesValueNum = usesValueNumber(candidate, valueNumber);

         // Check if this is a class load for a finalizable test
         if (usesValueNum &&
             node->getSymbolReference()->getOffset() < (int32_t)comp()->fej9()->getObjectHeaderSizeInBytes() &&
             node->getSymbolReference()->getSymbol()->isClassObject() &&
             node->getOpCode().isLoadIndirect() &&
             isFinalizableInlineTest(comp(), node->getFirstChild(), _curTree->getNode(), node))
            {
            // This transformation is optional for candidates that can't be stack-allocated and contiguous allocations,
            // but required for discontiguous allocations because the class field will no longer exist
            if (!candidate->isLocalAllocation() || candidate->isContiguousAllocation())
               {
               if (performTransformation(comp(), "%sRemoving inline finalizable test [%p] for candidate [%p]\n", OPT_DETAILS, _curTree->getNode(), candidate->_node))
                  removeThisNode = true;
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "Removing inline finalizable test [%p] for discontiguous candidate [%p]\n", _curTree->getNode(), candidate->_node);
               removeThisNode = true;
               }

            if (removeThisNode)
               {
               // The isFinalizable test results in zero because the candidate
               // has no finalizer, so we can turn it into a const 0.
               //
               // NOTE: Normally you'd want to anchor children here.  However,
               // one child is a const whose evaluation point doesn't matter.
               // The other is a dereference chain from the candidate.  If the
               // candidate is noncontiguously allocated, we must remove all
               // references, so it would be incorrect to anchor.  If the
               // candidate is contiguous, it will turn into a loadaddr, whose
               // evaluation point doesn't matter.  Since anchoring is
               // sometimes wrong and never necessary, we just don't do it.
               //
               TR::Node *andNode = _curTree->getNode()->getFirstChild();
               andNode->removeAllChildren();


               // In 64-bit mode the isFinalizable test will start out using
               // long operations, but subsequent optimization might reduce
               // them to int operations, so be prepared to replace the and
               // operation with a zero of the appropriate size
               if (andNode->getOpCodeValue() == TR::land)
                  {
                  TR::Node::recreate(andNode, TR::lconst);
                  andNode->setLongInt(0);
                  }
               else if (andNode->getOpCodeValue() == TR::iand)
                  {
                  TR::Node::recreate(andNode, TR::iconst);
                  andNode->setInt(0);
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Expected iand or land in isFinalizable test");
                  }
               }

            return false;
            }

         if (candidate->isLocalAllocation() && usesValueNum)
            {
            int32_t fieldOffset = node->getSymbolReference()->getOffset();
            if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
               {
               TR::SymbolReference *symRef = node->getSymbolReference();
               fieldOffset = symRef->getOffset();
               }
            else
               {
               TR::Node *offsetNode = NULL;
               if (node->getFirstChild()->getOpCode().isArrayRef())
                  offsetNode = node->getFirstChild()->getSecondChild();

               if (offsetNode && offsetNode->getOpCode().isLoadConst())
                  {
                  if (offsetNode->getType().isInt64())
                     fieldOffset = (int32_t) offsetNode->getLongInt();
                  else
                     fieldOffset = offsetNode->getInt();
                  }
               }

            // For a noncontiguous allocation, we can't leave behind any
            // dereferences of the candidate, so we must transform this
            // dereference.  If it's accessing a field that is not present in
            // the candidate, the code is already wrong (and presumably must
            // never run!), so we're ok to replace it with different code that
            // is also wrong.  Hence, we'll vandalize the tree into an iconst 0.
            //
            // NOTE: it would be cleaner to turn a field load into a const of
            // the right type, and a store into a treetop, rather than changing
            // the base pointer into an iconst 0.  However, we have done this
            // for years, so we'll leave it alone to reduce risk because we're
            // about to ship Java 8.
            //
            bool mustRemoveDereferences = !candidate->isContiguousAllocation();
            bool fieldIsPresentInObject = true;

            int32_t j;
            for (j = candidate->_fields->size()-1; j >= 0; j--)
               {
               if ((candidate->_fields->element(j)._offset == fieldOffset) &&
                   (!candidate->_fields->element(j).symRefIsForFieldInAllocatedClass(node->getSymbolReference())))
                  {
                  if (mustRemoveDereferences)
                     {
                     child->decReferenceCount();
                     child = TR::Node::create(child, TR::iconst, 0, 0);
                     if (trace())
                        {
                        traceMsg(comp(), "Change illegal deref %s [%p] to use dummy base %s [%p] in place of %s [%p]\n",
                           node->getOpCode().getName(), node,
                           child->getOpCode().getName(), child,
                           node->getChild(0)->getOpCode().getName(), node->getChild(0));
                        }
                     node->setAndIncChild(0, child);

                     // A wrtbari can also have the object as the last child.
                     // If we're vandalizing the wrtbari, it's because we've
                     // proven it's changing the candidate, and in that case,
                     // the third child must be a pointer to the candidate.  We
                     // can safely drop that child with no further checking.
                     //
                     static char *disableWrtbarFixing = feGetEnv("TR_disableWrtbarFixing");
                     if (node->getOpCode().isWrtBar() && !disableWrtbarFixing)
                        {
                        if (trace())
                           {
                           traceMsg(comp(), " -> Change %s [%p] into astorei, removing child %s [%p]\n",
                              node->getOpCode().getName(), node, node->getChild(2)->getOpCode().getName(), node->getChild(2));
                           }
                        node->getAndDecChild(2);
                        node->setNumChildren(2);
                        TR::Node::recreate(node, TR::astorei);
                        node->setFlags(0);

                        if (parent->getOpCode().isCheck() || parent->getOpCodeValue() == TR::compressedRefs)
                           {
                           if (trace())
                              traceMsg(comp(), " -> Eliminate %s [%p]\n", parent->getOpCode().getName(), parent);
                           TR::Node::recreate(parent, TR::treetop);
                           parent->setFlags(0);
                           }
                        }
                     }
                  fieldIsPresentInObject = false;
                  break;
                  }
               }

            if (fieldIsPresentInObject)
               {
               // For a candidate that is escaping in a cold block, keep track
               // of fields that are referenced so they can be initialized.
               // Otherwise, rewrite field references (in else branch_.
               // Special case handling of stores to fields of immutable objects
               // that are not contiguously allocated - their field references
               // are also rewritten (in else branch), but the original stores
               // still preserved.
               //
               if (candidate->escapesInColdBlock(_curBlock)
                      && (!isImmutableObject(candidate)
                             || candidate->isContiguousAllocation()
                             || node->getOpCode().isLoadVar()))
                  {
                  // Uh, why are we re-calculating the fieldOffset?  Didn't we just do that above?
                  //
                  int32_t fieldOffset = node->getSymbolReference()->getOffset();
                  if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
                     {
                     TR::SymbolReference *symRef = node->getSymbolReference();
                     fieldOffset = symRef->getOffset();
                     }
                  else
                     {
                     TR::Node *offsetNode = NULL;
                     if (node->getFirstChild()->getOpCode().isArrayRef())
                        offsetNode = node->getFirstChild()->getSecondChild();

                     if (offsetNode && offsetNode->getOpCode().isLoadConst())
                        {
                        if (offsetNode->getType().isInt64())
                          fieldOffset = (int32_t) offsetNode->getLongInt();
                       else
                          fieldOffset = offsetNode->getInt();
                       }
                     }

                  if (TR_yes == candidateHasField(candidate, node, fieldOffset, this))
                     {
                     // Remember the symbol reference of this field so that it can be used to
                     // zero-initialize the object.
                     //
                     int32_t i;
                     TR::SymbolReference *symRef = node->getSymbolReference();

                     int32_t nodeSize = node->getSize();
                     if (comp()->useCompressedPointers() &&
                           (node->getDataType() == TR::Address))
                        nodeSize = TR::Compiler->om.sizeofReferenceField();

                     if (fieldOffset + nodeSize <= candidate->_size) // cmvc 200318: redundant but harmless -- leave in place to minimize disruption
                        {
                        for (i = candidate->_fields->size()-1; i >= 0; i--)
                           {
                           if (candidate->_fields->element(i)._offset == fieldOffset)
                              {
                              candidate->_fields->element(i).rememberFieldSymRef(node, candidate, this);
                              if (candidate->isContiguousAllocation())
                                 candidate->_fields->element(i)._symRef = symRef;
                              candidate->_fields->element(i)._vectorElem = 0;

                              break;
                              }
                          }
                        TR_ASSERT(i >= 0, "assertion failure");
                        }
                     }
                  }

               // For a candidate that is not escaping in a cold block, or that
               // is an immutable object that is non-contiguously allocated,
               // rewrite references
               //
               else // if (!candidate->escapesInColdBlock(_curBlock))
                    //        || (isImmutableObject(candidate)
                    //               && !candidate->isContiguousAllocation()))
                    //               && !node->getOpCode().isLoadVar()))
                  {
                  if (candidate->isContiguousAllocation())
                     removeThisNode |= fixupFieldAccessForContiguousAllocation(node, candidate);
                  else
                     {
                     removeThisNode |= fixupFieldAccessForNonContiguousAllocation(node, candidate, parent);
                     break; // Can only be one matching candidate
                     }
                  }
               }
            else   //There was a field outside the bound of an object
               if (!candidate->isContiguousAllocation())
                  break; // Can only be one matching candidate
            }
         }

      if (removeThisNode)
         return true;
      }

   // Look for call to Throwable::fillInStackTrace that can be removed
   // (Note this has to be done before processing the children, since the
   // children will go away with the call.
   //
   if (node->getOpCode().isCall())
      {
      calledMethod = node->getSymbol()->getResolvedMethodSymbol();
      if (calledMethod && (!calledMethod->getResolvedMethod()->virtualMethodIsOverridden() || !node->getOpCode().isIndirect()))
         {
         if (calledMethod->getRecognizedMethod() == TR::java_lang_Throwable_fillInStackTrace)
            {
            child = node->getChild(node->getFirstArgumentIndex());
            valueNumber= _valueNumberInfo->getValueNumber(child);
            candidate = findCandidate(valueNumber);
            if (candidate && candidate->fillsInStackTrace() && !candidate->usesStackTrace() &&
                candidate->isLocalAllocation() && !candidate->escapesInColdBlocks() && performTransformation(comp(), "%sRemoving call node [%p] to fillInStackTrace\n",OPT_DETAILS, node))
               {
                 //printf("Removing fillInStackTrace call in %s\n", comp()->signature());

               // If the result of the call is not used, just remove the tree.
               // Otherwise replace the call with aconst 0
               //
               anchorCandidateReference(candidate, child);
               if (optimizer()->prepareForNodeRemoval(node, /* deferInvalidatingUseDefInfo = */ true))
                  _invalidateUseDefInfo = true;
               if (node->getReferenceCount() == 1)
                  return true;
               node->removeAllChildren();
               TR::Node::recreate(node, TR::aconst);
               node->setInt(0);
               node->setIsNonNull(false);
               node->setIsNull(true);
               return false;
               }
            }
         }
      }

   // Handle some standard constant propagation cases that reference candidate
   // objects. Since we may be inlining and then doing another pass of escape
   // analysis without doing constant propagation in between, these cases may
   // appear even though constant propagation would have simplified them.
   // They are handled here so that extraneous references to the candidates are
   // removed.
   //
   else if (node->getOpCodeValue() == TR::NULLCHK)
      {
      // If the reference child of the NULLCHK is a candidate, replace the
      // NULLCHK node by a treetop.
      //
      child = node->getNullCheckReference();
      valueNumber = _valueNumberInfo->getValueNumber(child);
      candidate = findCandidate(valueNumber);
      if (candidate)
         {
         TR::Node::recreate(node, TR::treetop);
         }
      }

   else if (node->getOpCode().isArrayLength())
      {
      // If the child of the arraylength node is a candidate, replace the
      // arraylength node by the (constant) bound from the allocation node.
      //
      child = node->getFirstChild();
      valueNumber = _valueNumberInfo->getValueNumber(child);
      candidate = findCandidate(valueNumber);
      if (candidate &&
          performTransformation(comp(), "%sReplacing arraylength [%p] by constant %d\n", OPT_DETAILS, node, candidate->_node->getFirstChild()->getInt()))
         {
         //TR_ASSERT((candidate->_node->getOpCodeValue() != TR::New) && (candidate->_node->getOpCodeValue() != TR::newStructRef), "assertion failure");
         anchorCandidateReference(candidate, child);
         if (optimizer()->prepareForNodeRemoval(node, /* deferInvalidatingUseDefInfo = */ true))
            _invalidateUseDefInfo = true;
         node->removeAllChildren();
         TR::Node::recreate(node, TR::iconst);
         if (candidate->_node->getOpCodeValue() == TR::New
             || candidate->_node->getOpCodeValue() == TR::newvalue)
            {
            // Dead code: can't ever execute an arraylength on a TR::New or TR::newvalue
            //see ArrayLest.test_getLTR::java_lang_ObjectI for an example
            node->setInt(0xdeadc0de);
            }
         else
            {
            node->setInt(candidate->_node->getFirstChild()->getInt());
            }
         // No need to look at children
         //
         return false;
         }
      }

   else if (node->getOpCode().isCheckCast() ||
            node->getOpCodeValue() == TR::instanceof)
      {
      // If the first child is a candidate, decide if the test will succeed or
      // fail and change the node accordingly
      //
      child = node->getFirstChild();
      TR::Node *classNode  = node->getSecondChild();
      valueNumber = _valueNumberInfo->getValueNumber(child);
      candidate = findCandidate(valueNumber);
      if (candidate && isConstantClass(classNode, this))
         {
         anchorCandidateReference(candidate, child);
         if (optimizer()->prepareForNodeRemoval(node, /* deferInvalidatingUseDefInfo = */ true))
            _invalidateUseDefInfo = true;
         if (comp()->fej9()->isInstanceOf((TR_OpaqueClassBlock *) candidate->_class, (TR_OpaqueClassBlock*)classNode->getSymbol()->castToStaticSymbol()->getStaticAddress(), true) == TR_yes)
            {
            if (node->getOpCodeValue() == TR::instanceof)
               {
               if (performTransformation(comp(), "%sReplacing instanceof [%p] by constant 1\n", OPT_DETAILS, node))
                  {
                  node->removeAllChildren();
                  TR::Node::recreate(node, TR::iconst);
                  node->setInt(1);
                  }
               }
            else
               {
               if (performTransformation(comp(), "%sReplacing %s [%p] by TR::treetop\n", OPT_DETAILS, node->getOpCode().getName(), node))
                  {
                  optimizer()->getEliminatedCheckcastNodes().add(node);
                  optimizer()->getClassPointerNodes().add(classNode);
                  requestOpt(OMR::catchBlockRemoval);
                  removeThisNode = true;
                  }
               }
            }
         else
            {
            if (node->getOpCodeValue() == TR::instanceof)
               {
               if (performTransformation(comp(), "%sReplacing instanceof [%p] by constant 0\n", OPT_DETAILS, node))
                  {
                  node->removeAllChildren();
                  TR::Node::recreate(node, TR::iconst);
                  node->setInt(0);
                  }
               }
            else
               {
               // The checkcast is ok as-is, so long as we still have an object
               // around to do the checkcast on.
               //
               TR_ASSERT(!candidate->isLocalAllocation() || candidate->isContiguousAllocation() || candidate->objectIsReferenced(), "assertion failure");
               }
            }

         // No need to look at children
         //
         return removeThisNode;
         }
      }

   else if (node->getOpCodeValue() == TR::ifacmpeq || node->getOpCodeValue() == TR::ifacmpne)
      {
      // If one of the children is a candidate we may be able to determine if
      // the comparison will succeed or fail
      //
      int32_t firstValue  = _valueNumberInfo->getValueNumber(node->getFirstChild());
      int32_t secondValue = _valueNumberInfo->getValueNumber(node->getSecondChild());
      int32_t compareValue = -1;
      if (firstValue == secondValue)
         {
         compareValue = 0;
         }
      else
         {
         bool notEqual = false;
         candidate = findCandidate(firstValue);
         if (candidate && ((!candidate->_seenSelfStore && !candidate->_seenStoreToLocalObject && !candidate->escapesInColdBlocks() && !usesValueNumber(candidate, secondValue)) || (node->getSecondChild()->getOpCodeValue() == TR::aconst)))
            notEqual = true;
         else
            {
            candidate = findCandidate(secondValue);
            if (candidate && ((!candidate->_seenSelfStore && !candidate->_seenStoreToLocalObject && !candidate->escapesInColdBlocks() && !usesValueNumber(candidate, firstValue)) || (node->getSecondChild()->getOpCodeValue() == TR::aconst)))
               notEqual = true;
            }
         if (notEqual)
            compareValue = 1;
         }

      if (compareValue >= 0 &&
          performTransformation(comp(), "%sChanging compare node [%p] so that constant propagation can predict it\n", OPT_DETAILS, node))
         {
         // The comparison is bound to succeed or fail.
         // Since it is complicated to remove code at this point, we simply
         // change the arguments so that constant propagation can predict that
         // the comparison will succeed or fail, and change the code accordingly
         //
         node->removeAllChildren();
         node->setNumChildren(2);
         node->setAndIncChild(0, TR::Node::aconst(node, 0));
         node->setAndIncChild(1, TR::Node::aconst(node, compareValue));
         _somethingChanged = true;

         // No need to look at children
         //
         return false;
         }
      }
   // Get rid of compressed ref anchors for loads/stores to fields of objects that have been made non-contiguous
   else if (comp()->useCompressedPointers() &&
            node->getOpCodeValue() == TR::compressedRefs)
      {
      TR::Node *loadOrStore = node->getFirstChild();
      bool treeAsExpected = false;
      if (loadOrStore &&
     (loadOrStore->getOpCode().isLoadVarOrStore() || loadOrStore->getOpCode().isLoadConst()))
     treeAsExpected = true;

      if (!treeAsExpected || (loadOrStore->getOpCode().hasSymbolReference() && loadOrStore->getSymbolReference()->getSymbol()->isAuto()))
         {
         TR::Node::recreate(node, TR::treetop);
         node->getSecondChild()->decReferenceCount();
         node->setSecond(NULL);
         node->setNumChildren(1);
         if (loadOrStore->getOpCode().isStore())
            {
            // Explicitly remove stores; some opts (e.g. DSE) don't expect commoned stores.
            node->setFirst(loadOrStore->getFirstChild());
            if (loadOrStore->getReferenceCount() > 1)
               {
               TR_ASSERT(loadOrStore->getFirstChild(), "Expecting store to have a child.");
               loadOrStore->getFirstChild()->incReferenceCount();
               }
            loadOrStore->decReferenceCount();
            if (trace())
               traceMsg(comp(), "Changing orphaned compressed ref anchor [%p] to auto to a treetop and removing store [%p].\n", node, loadOrStore);
            }
         else
            {
            if (trace())
               traceMsg(comp(), "Changing orphaned compressed ref anchor [%p] to auto to a treetop.\n", node);
            }
         return false;
         }
      }

   // Look for opportunities to de-synchronize references to a candidate.
   //
   TR::Node * synchronizedObject = 0;
#if CHECK_MONITORS
   if (_removeMonitors &&
       (node->getOpCodeValue() == TR::monent ||
        node->getOpCodeValue() == TR::monexit))
      synchronizedObject = node->getFirstChild();
#else
   if (node->getOpCodeValue() == TR::monent ||
       node->getOpCodeValue() == TR::monexit)
      synchronizedObject = node->getFirstChild();
#endif
   else if (node->getOpCodeValue() == TR::allocationFence)
      synchronizedObject = node->getAllocation();
   else if (calledMethod && calledMethod->isSynchronised() && !calledMethod->isStatic())
      synchronizedObject = node->getChild(node->getFirstArgumentIndex());

   if (synchronizedObject)
      {
      valueNumber = _valueNumberInfo->getValueNumber(synchronizedObject);
      candidate = findCandidate(valueNumber);
      if (candidate &&
          !candidate->escapesInColdBlocks())
         {
         if (calledMethod)
            {
            if (_desynchronizeCalls)
               {
               if (trace())
                  {
                  /////printf("sec Opportunity to desynchronize call to %s (size %d) in %s\n", calledMethod->getResolvedMethod()->signature(trMemory()), maxBytecodeIndex(calledMethod->getResolvedMethod()), comp()->signature());
                  traceMsg(comp(), "Mark call node [%p] as desynchronized\n", node);
                  }
               node->setDesynchronizeCall(true);
               if (!_inlineCallSites.find(_curTree))
                  _inlineCallSites.add(_curTree);
               }
            }
         else
            {
#if CHECK_MONITORS
            if (trace())
               traceMsg(comp(), "Remove redundant monitor node [%p]\n", node);
            removeThisNode = true;
#else
            if (node->getOpCodeValue() == TR::allocationFence)
               {
               if (candidate->isLocalAllocation())
                  {
                  if (trace())
                     traceMsg(comp(), "Redundant flush node [%p] found for candidate [%p]! Set omitSync flag on redundant flush node.\n", node, candidate->_node);

                  node->setOmitSync(true);
                  node->setAllocation(NULL);
                  //removeThisNode = true;
                  }
               }
            else if (candidate->isLocalAllocation())
               {
               // Mark the node as being a monitor on a local object and let
               // redundant monitor elimination get rid of it.
               //
               node->setLocalObjectMonitor(true);
               requestOpt(OMR::redundantMonitorElimination);
               if (trace())
                  traceMsg(comp(), "Mark monitor node [%p] for candidate [%p] as local object monitor\n", node, candidate->_node);
               }
#endif
            }
         }

      else if(candidate && candidate->escapesInColdBlocks() &&
              node->getOpCodeValue() == TR::allocationFence &&
              candidate->isLocalAllocation())
         {
         ListIterator<TR_ColdBlockEscapeInfo> coldBlkInfoIt(candidate->getColdBlockEscapeInfo());
         TR_ColdBlockEscapeInfo *info;
         for (info = coldBlkInfoIt.getFirst(); info != NULL; info = coldBlkInfoIt.getNext())
            {
            TR::Block *coldBlk = info->getBlock();
            if (!hasFlushOnEntry(coldBlk->getNumber()))
               {
               TR::TreeTop *insertionPoint = coldBlk->getEntry();
               TR::Node *flush = TR::Node::createAllocationFence(candidate->_node, candidate->_node);
               flush->setAllocation(NULL);
               TR::TreeTop *flushTT = TR::TreeTop::create(comp(), flush, NULL, NULL);
               TR::TreeTop *afterInsertionPoint = insertionPoint->getNextTreeTop();
               flushTT->join(afterInsertionPoint);
               insertionPoint->join(flushTT);
               if (trace())
                  traceMsg(comp(), "Adding flush node %p for candidate %p to cold block_%d\n", flush, candidate->_node, coldBlk->getNumber());
               setHasFlushOnEntry(coldBlk->getNumber());
               }
            }
         if (trace())
            traceMsg(comp(), "Remove redundant flush node [%p] for candidate [%p]\n", node, candidate->_node);
         removeThisNode = true;

         }

      }

   for (i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      if (!visited.contains(child))
         {
         if (fixupNode(child, node, visited))
            removeThisNode = true;
         }
      }

   if (removeThisNode)
      return true;

   // If no local object is to be created remove all nodes that refer to the
   // original allocation.
   //
   candidate = findCandidate(_valueNumberInfo->getValueNumber(node));
   if (candidate &&
       candidate->isLocalAllocation() &&
       !candidate->isContiguousAllocation() &&
       //shouldRemoveTree(candidate, _curBlock) &&
       !candidate->objectIsReferenced() &&
       !comp()->suppressAllocationInlining())
      {
      // Remove trees (other than the allocation tree itself) that
      // refer to the allocation node.
      //
      if (_curTree != candidate->_treeTop)
         {
         // special handling for stores to the monitoredObject slot
         // if the object is going to be non-contiguously allocated,
         // then turn the store into a store of NULL. the store cannot be
         // removed because this would lead to an imbalance of the monitorStack
         // during liveMonitor propagation
         //
         if (_curTree->getNode()->getOpCode().isStore() &&
             (_curTree->getNode()->getSymbol()->holdsMonitoredObject() ||
              (_curTree->getNode()->getSymbolReference() == comp()->getSymRefTab()->findThisRangeExtensionSymRef())))
            {
            _curTree->getNode()->getFirstChild()->decReferenceCount();
            TR::Node *aconstNode = TR::Node::aconst(_curTree->getNode(), 0);
            _curTree->getNode()->setAndIncChild(0, aconstNode);
            if (trace())
               traceMsg(comp(), "%sFixed up liveMonitor store [%p] for candidate [%p]\n", OPT_DETAILS, _curTree->getNode(), candidate->_node);
            removeThisNode = false;
            }
         else
            {
            removeThisNode = true;

            if (trace())
               traceMsg(comp(), "Remove tree [%p] with direct reference to candidate [%p]\n", _curTree->getNode(), candidate->_node);
            }
         }

      // Reset the visit count on this node so that subsequent uses
      // are also removed.
      //
      visited.remove(node);
      }

   return removeThisNode;
   }


bool TR_EscapeAnalysis::fixupFieldAccessForContiguousAllocation(TR::Node *node, Candidate *candidate)
   {
   // Ignore stores to the generic int shadow for nodes that are already
   // explicitly initialized. These are the initializing stores and are going to
   // be left as they are (except maybe to insert real field symbol references
   // later if we can).
   //
   if (candidate->isExplicitlyInitialized() &&
       node->getSymbol() == getSymRefTab()->findGenericIntShadowSymbol())
      {
      return false;
      }

   // If the local allocation is the only object that can be the base
   // of a write barrier store, change it into a simple indirect store.
   // We should be able to do this for arrays as well, however, IA32 TreeEvaluator needs to be
   // fixed in order to support this. FIXME
   //
   if (node->getOpCode().isWrtBar() &&
       !candidate->escapesInColdBlocks() &&
       _valueNumberInfo->getValueNumber(node->getFirstChild()) == _valueNumberInfo->getValueNumber(candidate->_node))
      {
        if (candidate->_origKind == TR::New)
         {
         TR::Node::recreate(node, TR::astorei);
         node->getChild(2)->recursivelyDecReferenceCount();
         node->setNumChildren(2);
         _repeatAnalysis = true;
         if (trace())
            traceMsg(comp(), "Change node [%p] from write barrier to regular store\n", node);
         }
      else
         {
         // We do not remove the wrtbars yet - but atleast remove the bits from
         // them indicating that they are non-heap wrtbars
         //
         node->setIsHeapObjectWrtBar(false);
         node->setIsNonHeapObjectWrtBar(true);
         }
      }

   int32_t fieldOffset = node->getSymbolReference()->getOffset();
   if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      fieldOffset = symRef->getOffset();
      }
   else
      {
      TR::Node *offsetNode = NULL;
      if (node->getFirstChild()->getOpCode().isArrayRef())
          offsetNode = node->getFirstChild()->getSecondChild();

      if (offsetNode && offsetNode->getOpCode().isLoadConst())
         {
         if (offsetNode->getType().isInt64())
            fieldOffset = (int32_t) offsetNode->getLongInt();
         else
            fieldOffset = offsetNode->getInt();
         }
      }

   // Remember the symbol reference of this field so that it can be used to
   // zero-initialize the object.
   // Only do this for fields whose offsets are within the bounds of the object.
   // It is possible to reach uses which (via downcasting) are referencing a
   // derived class from the candidate class. At run time, the local allocation
   // should never reach these program points.
   //
   if (TR_yes == candidateHasField(candidate, node, fieldOffset, this))
      {
      // Remember the symbol reference of this field so that it can be used to
      // zero-initialize the object.
      //
      int32_t i;
      TR::SymbolReference *symRef = node->getSymbolReference();

      int32_t nodeSize = node->getSize();
      if (comp()->useCompressedPointers() &&
            (node->getDataType() == TR::Address))
         nodeSize = TR::Compiler->om.sizeofReferenceField();

      if (fieldOffset + nodeSize <= candidate->_size) // cmvc 200318: redundant but harmless -- leave in place to minimize disruption
         {
         for (i = candidate->_fields->size()-1; i >= 0; i--)
            {
            if (candidate->_fields->element(i)._offset == fieldOffset)
               {
               candidate->_fields->element(i).rememberFieldSymRef(node, candidate, this);
               candidate->_fields->element(i)._symRef = symRef;
               candidate->_fields->element(i)._vectorElem = 0;
               break;
               }
            }
         TR_ASSERT(i >= 0, "candidate [%p] must have field (%s) at offset %d due to %p.  Scan for escapes missed a field reference!\n", candidate->_node, comp()->getDebug()->getName(symRef), fieldOffset, node);
         }
      }
   return false;
   }

//We should provide a bound for the following array
//Where are the following hard-coded constants coming from anyway?
static TR::DataType convertArrayTypeToDataType[] =
   {
   TR::NoType, //0
   TR::NoType, //1
   TR::NoType, //2
   TR::NoType, //3
   TR::Int8,   //4
   TR::Int16, //5
   TR::Float,  //6
   TR::Double, //7
   TR::Int8,  //8
   TR::Int16,//9
   TR::Int32,     //10
   TR::Int64
   };

TR::Node *TR_EscapeAnalysis::createConst(TR::Compilation *comp, TR::Node *node, TR::DataType type, int value)
   {
   TR::Node *result;

   if (type.isVector())
      {
      result = TR::Node::create(node, TR::ILOpCode::createVectorOpCode(TR::vsplats, type), 1);
      result->setAndIncChild(0, TR::Node::create(node, comp->il.opCodeForConst(type.getVectorElementType()), value));
      }
   else
      {
      result = TR::Node::create(node, comp->il.opCodeForConst(type), value);
      }
   return result;
   }


bool TR_EscapeAnalysis::fixupFieldAccessForNonContiguousAllocation(TR::Node *node, Candidate *candidate, TR::Node *parent)
   {
   int32_t i;
   int32_t fieldOffset = (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
                            ? comp()->fej9()->getObjectHeaderSizeInBytes()
                            : TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   TR::DataType fieldType = TR::NoType; // or array element type

   // If this is a store to the generic int shadow, it is zero-initializing the
   // object. Remember which words are being zero-initialized; only fields that
   // intersect with these words will have to be explicitly zero-initialized.
   // These initializing stores can then be thrown away.
   //
   if (candidate->isExplicitlyInitialized() &&
       node->getOpCode().isStore() &&
       node->getSymbol() == getSymRefTab()->findGenericIntShadowSymbol())
      {
      if (!candidate->_initializedWords)
         candidate->_initializedWords = new (trStackMemory()) TR_BitVector(candidate->_size,  trMemory(), stackAlloc);

      for (i = 3; i >= 0; i--)
         candidate->_initializedWords->set(node->getSymbolReference()->getOffset()+i);

      if (trace())
         traceMsg(comp(), "Remove explicit new initialization node [%p]\n", node);
      return true;
      }

   if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      fieldOffset = symRef->getOffset();
      fieldType   = symRef->getSymbol()->getDataType();
      }
   else
      {
      TR_ASSERT(node->getSymbolReference()->getSymbol()->isArrayShadowSymbol(), "expecting store to an array shadow");
      fieldOffset = node->getSymbolReference()->getOffset();
      TR::Node *typeNode   = candidate->_node->getSecondChild();
      TR::Node *offsetNode = NULL;
      if (node->getFirstChild()->getOpCode().isArrayRef())
         offsetNode = node->getFirstChild()->getSecondChild();

      if (offsetNode && offsetNode->getOpCode().isLoadConst())
         {
         if (offsetNode->getType().isInt64())
            fieldOffset = (int32_t) offsetNode->getLongInt();
         else
            fieldOffset = offsetNode->getInt();
         }

      if (candidate->_origKind == TR::newarray)
         fieldType = convertArrayTypeToDataType[typeNode->getInt()];
      else
         fieldType = TR::Address;
      }

   // This can happen for a vft-symbol which has no type
   //
   if (fieldType == TR::NoType)
      fieldType = TR::Address;

   // Find or create the auto that is to replace this field reference
   //
   TR::SymbolReference *autoSymRef = NULL;
   for (i = candidate->_fields->size()-1; i >= 0; i--)
      {
      if (candidate->_fields->element(i)._offset == fieldOffset)
         {
         autoSymRef = candidate->_fields->element(i)._symRef;
         break;
         }
      }

   //TR_ASSERT(i >= 0, "assertion failure");

   if (i >= 0)
      {
      // Change the load or store to be a direct load or store of the
      // auto. We may have to introduce a conversion if the opcode type for the
      // direct store is different from the opcode type for the indirect store
      //
      TR::DataType nodeType = node->getDataType();
      TR::ILOpCodes newOpCode;
      if (node->getOpCode().isLoadVar())
         newOpCode = comp()->il.opCodeForDirectLoad(nodeType);
      else
         newOpCode = comp()->il.opCodeForDirectStore(nodeType);
      TR::DataType newOpType = comp()->fej9()->dataTypeForLoadOrStore(nodeType);
      TR::ILOpCodes conversionOp;

      int elem = candidate->_fields->element(i)._vectorElem;
      if (elem != 0)
         {
         fieldOffset = fieldOffset - TR::Symbol::convertTypeToSize(nodeType)*(elem-1);
         autoSymRef = NULL;
         for (i = candidate->_fields->size()-1; i >= 0; i--)
            {
            if (candidate->_fields->element(i)._offset == fieldOffset)
               {
               autoSymRef = candidate->_fields->element(i)._symRef;
               break;
               }
            }
         TR_ASSERT(i >= 0, "element 0 should exist\n");

         if (!newOpType.isVector())
            newOpType = newOpType.scalarToVector(TR::VectorLength128); // TODO: use best vector length available

         TR_ASSERT(newOpType != TR::NoType, "wrong type at node %p\n", node);
         }

      if (!autoSymRef)
         {
         autoSymRef = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), newOpType);
         autoSymRef->getSymbol()->setBehaveLikeNonTemp();
         candidate->_fields->element(i).rememberFieldSymRef(node, candidate, this);
         candidate->_fields->element(i)._symRef = autoSymRef;
         }

      if (node->getOpCode().isLoadVar())
         {
         node->removeAllChildren();
         conversionOp = TR::ILOpCode::getProperConversion(newOpType, nodeType, false /* !wantZeroExtension */);

         if (conversionOp != TR::BadILOp)
            {
            TR::Node::recreate(node, conversionOp);
            node->setAndIncChild(0, TR::Node::createWithSymRef(node, newOpCode, 0, autoSymRef));
            node->setNumChildren(1);
            }
         else
            {
            TR::Node::recreate(node, newOpCode);
            node->setSymbolReference(autoSymRef);
            }

         TR::DataType autoSymRefDataType = autoSymRef->getSymbol()->getDataType();

         if (autoSymRefDataType.isVector() &&
             !node->getDataType().isVector())
            {
            TR::Node::recreate(node, TR::ILOpCode::createVectorOpCode(TR::vgetelem, autoSymRefDataType));
            node->setAndIncChild(0, TR::Node::create(node, TR::ILOpCode::createVectorOpCode(TR::vload, autoSymRefDataType), 0));
            node->setNumChildren(2);
            node->getFirstChild()->setSymbolReference(autoSymRef);
            node->setAndIncChild(1, TR::Node::create(node, TR::iconst, 0, elem-1));
            }
         }
      else
         {
         conversionOp = TR::ILOpCode::getProperConversion(nodeType, newOpType, false /* !wantZeroExtension */);

         TR::Node *valueChild;
         if (conversionOp != TR::BadILOp)
            valueChild = TR::Node::create(conversionOp, 1, node->getSecondChild());
         else
            valueChild = node->getSecondChild();

         // Special case of non-contiguous immutable object that escapes in
         // a cold block:  need to ensure the store to the original object
         // is preserved for heapification; otherwise, replace the old store
         // completely
         if (candidate->escapesInColdBlock(_curBlock)
                && isImmutableObject(candidate) && !candidate->isContiguousAllocation())
            {
            TR::Node *newStore = TR::Node::createWithSymRef(newOpCode, 1, 1, valueChild, autoSymRef);
            TR::TreeTop *newTree = TR::TreeTop::create(comp(), newStore);
            TR::TreeTop *prev = _curTree->getPrevTreeTop();
            prev->join(newTree);
            newTree->join(_curTree);

            if (trace())
               traceMsg(comp(), "Preserve old node [%p] for store to non-contiguous immutable object that escapes in cold block; create new tree [%p] for direct store\n", node, newTree);
            }
         else
            {
            valueChild->incReferenceCount();
            node->removeAllChildren();
            node->setFirst(valueChild);
            node->setNumChildren(1);
            TR::Node::recreate(node, newOpCode);
            node->setSymbolReference(autoSymRef);
            }

         TR::DataType autoSymRefDataType = autoSymRef->getSymbol()->getDataType();

         if (autoSymRefDataType.isVector() &&
             !node->getDataType().isVector())
            {
            TR::Node::recreate(node, TR::ILOpCode::createVectorOpCode(TR::vstore, autoSymRefDataType));
            TR::Node *value = node->getFirstChild();
            TR::Node *newValue = TR::Node::create(node, TR::ILOpCode::createVectorOpCode(TR::vsetelem, autoSymRefDataType), 3);
            newValue->setAndIncChild(0, TR::Node::create(node, TR::ILOpCode::createVectorOpCode(TR::vload, autoSymRefDataType), 0));
            newValue->getFirstChild()->setSymbolReference(autoSymRef);
            newValue->setChild(1, value);
            newValue->setAndIncChild(2, TR::Node::create(node, TR::iconst, 0, elem-1));
            node->setAndIncChild(0, newValue);
            }
         }
      if (trace())
         traceMsg(comp(), "Change node [%p] into a direct load or store of #%d (%d bytes) field %d cand %p\n", node, autoSymRef->getReferenceNumber(), autoSymRef->getSymbol()->getSize(), i, candidate);

      if (parent)
         {
         if (parent->getOpCode().isNullCheck())
            TR::Node::recreate(parent, TR::treetop);
         else if (parent->getOpCode().isSpineCheck() && (parent->getFirstChild() == node))
       {
            TR::TreeTop *prev = _curTree->getPrevTreeTop();

            int32_t i = 1;
            while (i < parent->getNumChildren())
          {
               TR::TreeTop *tt = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, parent->getChild(i)));
               parent->getChild(i)->recursivelyDecReferenceCount();
               prev->join(tt);
               tt->join(_curTree);
               prev = tt;
               i++;
          }

            TR::Node::recreate(parent, TR::treetop);
            parent->setNumChildren(1);
       }
         else if (parent->getOpCodeValue() == TR::ArrayStoreCHK)
            {
            TR::Node::recreate(parent, TR::treetop);

            // we need to prepend a _special_ check-cast node to make sure that the store
            // would be valid at runtime.  The checkcast is special only because it throws
            // ArrayStoreException instead of ClassCastException.
            //
            TR::Node *typeNode  = TR::Node::copy(candidate->_node->getSecondChild()); // loadaddr of array type
            typeNode->setReferenceCount(0);
            TR::Node *source    = node->getFirstChild();
            TR::Node *checkNode =
               TR::Node::createWithSymRef(TR::checkcast, 2, 2, source, typeNode,
                                 getSymRefTab()->findOrCreateCheckCastForArrayStoreSymbolRef(0));

            TR::TreeTop *prev = _curTree->getPrevTreeTop();
            TR::TreeTop *tt = TR::TreeTop::create(comp(), checkNode);
            prev->join(tt);
            tt->join(_curTree);
            }
         else if (parent->getOpCode().isAnchor())
            {
            TR::Node::recreate(parent, TR::treetop);
            parent->getSecondChild()->recursivelyDecReferenceCount();
            parent->setNumChildren(1);
            }
         }
      }
   else
      {
      // The load/store is in an unreachable portion of
      // the method. If it is a store we get rid of the treetop completely,
      // otherwise change the indirect load to be a constant of the
      // correct type
      //
      if (node->getOpCode().isStore())
         return true;
      else
         {
         TR::Node::recreate(node, comp()->il.opCodeForConst(node->getDataType()));
         if (node->getNumChildren() > 0)
            node->getFirstChild()->recursivelyDecReferenceCount();
         node->setLongInt(0);
         node->setNumChildren(0);
         if (trace())
            traceMsg(comp(), "Change node [%p] into a constant\n", node);
         }
      }

   return false;
   }


void TR_EscapeAnalysis::makeLocalObject(Candidate *candidate)
   {
   int32_t             i;
   TR::SymbolReference *symRef;
   TR::Node            *allocationNode = candidate->_node;

   // Change the "new" node into a load address of a local object/array
   //
   int32_t *referenceSlots = NULL;
   if (candidate->_kind == TR::New || candidate->_kind == TR::newvalue)
      {
      symRef = getSymRefTab()->createLocalObject(candidate->_size, comp()->getMethodSymbol(), allocationNode->getFirstChild()->getSymbolReference());

#if LOCAL_OBJECTS_COLLECTABLE
      if (candidate->isContiguousAllocation())
         referenceSlots = comp()->fej9()->getReferenceSlotsInClass(comp(), (TR_OpaqueClassBlock *)candidate->_node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress());
#endif
      if (!referenceSlots)
         symRef->getSymbol()->setNotCollected();
      else
         symRef->getSymbol()->getLocalObjectSymbol()->setReferenceSlots(referenceSlots);
      }
   else if (candidate->_kind == TR::anewarray)
      {
      symRef = getSymRefTab()->createLocalAddrArray(candidate->_size, comp()->getMethodSymbol(), allocationNode->getSecondChild()->getSymbolReference());
      symRef->setStackAllocatedArrayAccess();

      // Set up the array of reference slots
      //
      int32_t numSlots = 0;
#if LOCAL_OBJECTS_COLLECTABLE
      if (candidate->isContiguousAllocation())
         ////numSlots = (candidate->_size - TR::Compiler->om.contiguousArrayHeaderSizeInBytes()) / _cg->sizeOfJavaPointer();
         numSlots = (candidate->_size - TR::Compiler->om.contiguousArrayHeaderSizeInBytes()) / TR::Compiler->om.sizeofReferenceField();
#endif
      if (numSlots == 0)
         symRef->getSymbol()->setNotCollected();
      else
         {
         referenceSlots = (int32_t *)trMemory()->allocateHeapMemory((numSlots+1)*4, TR_Memory::EscapeAnalysis);
         ////int32_t hdrSlots = TR::Compiler->om.contiguousArrayHeaderSizeInBytes()/_cg->sizeOfJavaPointer();
         int32_t hdrSlots = TR::Compiler->om.contiguousArrayHeaderSizeInBytes()/TR::Compiler->om.sizeofReferenceField();
         for (i = 0; i < numSlots; i++)
            referenceSlots[i] = hdrSlots + i;
         referenceSlots[numSlots] = 0;
         symRef->getSymbol()->getLocalObjectSymbol()->setReferenceSlots(referenceSlots);
         }
      }
   else
      {
      symRef = getSymRefTab()->createLocalPrimArray(candidate->_size, comp()->getMethodSymbol(), allocationNode->getSecondChild()->getInt());
      symRef->setStackAllocatedArrayAccess();
      }

   if (trace() && referenceSlots)
      {
      traceMsg(comp(), "  Reference slots for candidate [%p] : {",candidate->_node);
      for (i = 0; referenceSlots[i]; i++)
         {
         traceMsg(comp(), " %d", referenceSlots[i]);
         }
      traceMsg(comp(), " }\n");
      }

   // Initialize the header of the local object
   //
   TR::Node *nodeToUseInInit = allocationNode->duplicateTree();
   TR::TreeTop *insertionPoint = comp()->getStartTree();

   if (candidate->_kind == TR::New || candidate->_kind == TR::newvalue)
      comp()->fej9()->initializeLocalObjectHeader(comp(), nodeToUseInInit, insertionPoint);
   else
      comp()->fej9()->initializeLocalArrayHeader(comp(), nodeToUseInInit, insertionPoint);

   allocationNode->removeAllChildren();
   TR::Node::recreate(allocationNode, TR::loadaddr);
   allocationNode->setSymbolReference(symRef);

   // Insert debug counter for a contiguous allocation.  Counter name is of the form:
   //
   //    escapeAnalysis/contiguous-allocation/<hotness>/<outermost-method-sig>/(<inlined-method-sig>)/(<bc-caller-index,bc-offset>)
   //
   // If the node is not from an inlined method, <inlined-method-sig> repeats the outermost method signature
   //
   TR_ByteCodeInfo bcInfo = allocationNode->getByteCodeInfo();

   const char *dbgCntName = TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/contiguous-allocation/%s/%s/(%s)/(%d,%d)",
                                 comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(),
                                 (bcInfo.getCallerIndex() > -1)
                                    ? comp()->getInlinedResolvedMethod(bcInfo.getCallerIndex())->signature(trMemory())
                                    : comp()->signature(),
                                 bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

   TR::DebugCounter::prependDebugCounter(comp(), dbgCntName, candidate->_treeTop);

   if (candidate->_seenArrayCopy || candidate->_argToCall || candidate->_seenSelfStore || candidate->_seenStoreToLocalObject)
      {
      allocationNode->setCannotTrackLocalUses(true);
      if (candidate->callsStringCopyConstructor())
         allocationNode->setCannotTrackLocalStringUses(true);
      }

   if (nodeToUseInInit != allocationNode)
      {
      nodeToUseInInit->removeAllChildren();
      TR::Node::recreate(nodeToUseInInit, TR::loadaddr);
      nodeToUseInInit->setSymbolReference(symRef);
      if (candidate->escapesInColdBlocks() || candidate->_seenArrayCopy || candidate->_argToCall || candidate->_seenSelfStore || candidate->_seenStoreToLocalObject)
         {
         if (candidate->escapesInColdBlocks())
            nodeToUseInInit->setEscapesInColdBlock(true);
         nodeToUseInInit->setCannotTrackLocalUses(true);
         if (candidate->callsStringCopyConstructor())
            nodeToUseInInit->setCannotTrackLocalStringUses(true);
         }
      }
   }



void TR_EscapeAnalysis::avoidStringCopyAllocation(Candidate *candidate)
   {
   if (comp()->suppressAllocationInlining())
      return;

   TR::Node *allocationNode = candidate->_node;

   dumpOptDetails(comp(), "%sReplacing new (String) node [%p] with the String that was used in the copy constructor\n",OPT_DETAILS, candidate->_node);

   if (trace() || debug ("traceContiguousESC"))
      {
      traceMsg(comp(), "secs (%d) String (copy) allocation of size %d found in %s\n", manager()->numPassesCompleted(), candidate->_size, comp()->signature());
      }


   TR::TreeTop *insertionPoint = candidate->_treeTop;
   TR::DataType dataType = candidate->_stringCopyNode->getDataType();
   TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType);
   TR::Node *initNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(candidate->_stringCopyNode->getDataType()), 1, 1, candidate->_stringCopyNode, newSymbolReference);
   TR::TreeTop *initTree = TR::TreeTop::create(comp(), initNode, 0, 0);
   TR::TreeTop *prevTree = insertionPoint->getPrevTreeTop();
   prevTree->join(initTree);
   initTree->join(insertionPoint);


   allocationNode->removeAllChildren();
   allocationNode->setNumChildren(0);
   TR::Node::recreate(allocationNode, comp()->il.opCodeForDirectLoad(candidate->_stringCopyNode->getDataType()));
   allocationNode->setSymbolReference(newSymbolReference);

   TR::TreeTop *stringInitCall = candidate->_stringCopyCallTree;

   if (stringInitCall)
      {
      TR::Node *stringInitCallNode = stringInitCall->getNode();
      stringInitCallNode->recursivelyDecReferenceCount();

      prevTree = stringInitCall->getPrevTreeTop();
      TR::TreeTop *nextTree = stringInitCall->getNextTreeTop();
      prevTree->join(nextTree);
      }
   }

/** \details
 *     This function will fully zero initialize the given candidate, meaning that aside from the header the entirety
 *     of the stack allocated object will be zero initialized. This function can handle both objects and arrays.
 */
bool TR_EscapeAnalysis::tryToZeroInitializeUsingArrayset(Candidate* candidate, TR::TreeTop* precedingTreeTop)
   {
   // TODO-VALUETYPE:  Investigate whether arrayset can be used for newvalue
   if (cg()->getSupportsArraySet() && candidate->_kind != TR::newvalue)
      {
      int32_t candidateHeaderSizeInBytes = candidate->_origKind == TR::New ? comp()->fej9()->getObjectHeaderSizeInBytes() : TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

      // Size of the object without the header
      int32_t candidateObjectSizeInBytes = candidate->_size - candidateHeaderSizeInBytes;

      if (candidateObjectSizeInBytes > 0)
         {
         TR::Node* allocationNode = candidate->_node;

         if (performTransformation(comp(), "%sUse arrayset to initialize [%p]\n", OPT_DETAILS, allocationNode))
            {
            TR::SymbolReference* allocationSymRef = allocationNode->getSymbolReference();

            TR::Node* arrayset = TR::Node::createWithSymRef(TR::arrayset, 3, 3,
               TR::Node::createWithSymRef(allocationNode, TR::loadaddr, 0, new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), allocationSymRef->getSymbol(), allocationSymRef->getOffset() + candidateHeaderSizeInBytes)),
               TR::Node::bconst(allocationNode, 0),
               TR::Node::iconst(allocationNode, candidateObjectSizeInBytes),
               comp()->getSymRefTab()->findOrCreateArraySetSymbol());

            TR::TreeTop* arraysetTreeTop = TR::TreeTop::create(comp(), precedingTreeTop, TR::Node::create(TR::treetop, 1, arrayset));

            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/zeroInitializeArrayset/%s", comp()->signature()), arraysetTreeTop);

            return true;
            }
         }
      }

   return false;
   }

void TR_EscapeAnalysis::makeNewValueLocalAllocation(Candidate *candidate)
   {
   //
   // Example of trees for a newvalue operation before stack allocation
   //
   //    112n     treetop
   //    n111n       newvalue  jitNewValue
   //    n108n         loadaddr  Pair
   //    n109n         iload x
   //    n119n         iconst 2 (X!=0 X>=0 )
   //
   // and after, for contiguous stack allocation, where #439 is aggregate storage on the stack.
   // The field holding the vft for the object will be inserted in the method entry block.
   //
   // In method entry block:
   //    n166n     astorei  <vft-symbol>[#345  Shadow]
   //    n160n       loadaddr  <temp slot 7>[#439  Auto]
   //    n165n       aiadd
   //    n161n         loadaddr  Pair
   //    n164n         iconst 0 (X==0 X>=0 X<=0 )
   //
   // Transformed IL for stack allocation of newvalue (contiguous)
   //    n158n     treetop
   //    n119n       iconst 2 (X!=0 X>=0 )
   //    n110n     treetop
   //    n109n       iload  x
   //    n112n     treetop
   //    n111n       loadaddr  <temp slot 7>[#439  Auto]
   //    n157n     istorei  Pair.x I[#429  final Shadow +4]
   //    n111n       ==>loadaddr
   //    n109n       ==>iload
   //    n159n     istorei  Pair.y I[#430  final Shadow +8]
   //    n111n       ==>loadaddr
   //    n119n       ==>iconst 2
   //
   // Or for non-contiguous stack allocation, #438 and #441 are local variables respectively
   // representing the Pair.x and Pair.y fields of the stack allocated value type instance
   //
   // Transformed IL for stack allocation of newvalue (non-contiguous)
   //    n110n     treetop
   //    n109n       iload  x
   //    n170n     istore  <temp slot 6>[#438  Auto]
   //    n109n       ==>iload
   //    n171n     istore  <temp slot 8>[#441  Auto]
   //    n99n        ==>iconst 2

   // TODO-VALUETYPE:  Can this code be commoned up with code in lowerNewValue?
   //
   TR_OpaqueClassBlock *valueClass = static_cast<TR_OpaqueClassBlock *>(candidate->_node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress());
   const TR::TypeLayout* typeLayout = comp()->typeLayout(valueClass);

   TR::TreeTop *fieldValueTreeTopCursor = candidate->_treeTop->getPrevTreeTop();
   TR::TreeTop *fieldStoreTreeTopCursor = candidate->_treeTop;

   // If the value type candidate escapes, all fields must be known
   // (i.e., candidate->_fields for newvalue must not be NULL or the newvalue
   // must be known to have no fields) so that the values of all fields can be set
   // properly for heapification at the escape point.
   // Ensure symbol references for all the fields are included in the list
   // associated with the candidate for stack allocation.
   //
   for (int i = 1; i < candidate->_node->getNumChildren(); i++)
      {
      TR::Node *fieldValueNode = candidate->_node->getChild(i);
      TR::Node *ttNode = TR::Node::create(TR::treetop, 1);
      ttNode->setAndIncChild(0, fieldValueNode);

      fieldValueTreeTopCursor = TR::TreeTop::create(comp(), fieldValueTreeTopCursor, ttNode);

      // generate store to the field
      const TR::TypeLayoutEntry& fieldEntry = typeLayout->entry(i - 1);
      TR::SymbolReference* symref = comp()->getSymRefTab()->findOrFabricateShadowSymbol(valueClass,
                                                                       fieldEntry._datatype,
                                                                       fieldEntry._offset,
                                                                       fieldEntry._isVolatile,
                                                                       fieldEntry._isPrivate,
                                                                       fieldEntry._isFinal,
                                                                       fieldEntry._fieldname,
                                                                       fieldEntry._typeSignature
                                                                       );

      int32_t fieldSize = TR::Symbol::convertTypeToSize(fieldEntry._datatype);

      if (fieldEntry._datatype.getDataType() == TR::Address && comp()->useCompressedPointers())
         {
         fieldSize = TR::Compiler->om.sizeofReferenceField();
         }

      TR::Node *completeFieldStoreNode = NULL;

      FieldInfo &field = candidate->findOrSetFieldInfo(NULL, symref, fieldEntry._offset, fieldSize, fieldEntry._datatype, this);

      if (candidate->isContiguousAllocation())
         {
         field._symRef = symref;
         field._vectorElem = 0;

         TR::DataType fieldDataType = fieldValueNode->getDataType();
         const bool useWriteBarrier = ( (fieldDataType == TR::Address)
                                        && (TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_none) )
                                      || comp()->getOption(TR_EnableFieldWatch);
         TR::Node *storeNode;

         if (useWriteBarrier)
            {
            const TR::ILOpCodes storeOpCode = comp()->il.opCodeForIndirectWriteBarrier(fieldValueNode->getDataType());
            storeNode = TR::Node::createWithSymRef(storeOpCode, 3, 3, candidate->_node, fieldValueNode, candidate->_node, symref);
            }
         else
            {
            const TR::ILOpCodes storeOpCode = comp()->il.opCodeForIndirectStore(fieldValueNode->getDataType());
            storeNode = TR::Node::createWithSymRef(storeOpCode, 2, 2, candidate->_node, fieldValueNode, symref);
            }

         // if storing a ref, make sure it is compressed
         if (comp()->useCompressedPointers() && fieldValueNode->getDataType() == TR::Address)
            {
            completeFieldStoreNode = TR::Node::createCompressedRefsAnchor(storeNode);
            }
         else
            {
            completeFieldStoreNode = storeNode;
            }
         }
      else
         {
         TR::SymbolReference *autoSymRef = field._symRef;

         if (autoSymRef == NULL || !autoSymRef->getSymbol()->isAuto())
            {
            autoSymRef = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), fieldEntry._datatype);
            autoSymRef->getSymbol()->setBehaveLikeNonTemp();
            field.rememberFieldSymRef(symref, this);
            field._symRef = autoSymRef;
            }

         TR::DataType type = autoSymRef->getSymbol()->getDataType();
         completeFieldStoreNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(type), 1, 1, fieldValueNode, autoSymRef);
         }
      fieldStoreTreeTopCursor = TR::TreeTop::create(comp(), fieldStoreTreeTopCursor, completeFieldStoreNode);
      }
   }

void TR_EscapeAnalysis::makeContiguousLocalAllocation(Candidate *candidate)
   {
   int32_t             i,j;
   int32_t             offset;
   TR::Node            *node;
   TR::Symbol           *sym;
   TR::TreeTop         *initTree, *next;

   if (comp()->suppressAllocationInlining())
      return;

   if (comp()->generateArraylets() && candidate->_kind != TR::New
       && candidate->_kind != TR::newvalue)
      return;

   dumpOptDetails(comp(), "%sMaking %s node [%p] into a local object of size %d\n",OPT_DETAILS, candidate->_node->getOpCode().getName(), candidate->_node, candidate->_size);

   if (trace() || debug ("traceContiguousESC"))
      {
      traceMsg(comp(), "secs (%d) Contiguous allocation of size %d found in %s\n", manager()->numPassesCompleted(), candidate->_size, comp()->signature());
      }

   if (candidate->escapesInColdBlocks())
      {
      candidate->_originalAllocationNode = candidate->_node->duplicateTree();
      }

   bool skipZeroInit = false;
   TR::ILOpCodes       originalAllocationOpCode = candidate->_node->getOpCodeValue();

   if ((originalAllocationOpCode != TR::New) && (originalAllocationOpCode != TR::newvalue) &&
       candidate->_node->canSkipZeroInitialization())
      skipZeroInit = true;

   if (candidate->isExplicitlyInitialized() && (originalAllocationOpCode == TR::newvalue))
      {
      makeNewValueLocalAllocation(candidate);
      }

   makeLocalObject(candidate);

   if (skipZeroInit)
      return;

   TR::Node            *allocationNode = candidate->_node;
   TR::SymbolReference *symRef = allocationNode->getSymbolReference();
   int32_t            *referenceSlots = symRef->getSymbol()->getLocalObjectSymbol()->getReferenceSlots();

   if (candidate->isExplicitlyInitialized())
      {
      if (originalAllocationOpCode != TR::newvalue)
         {
         // Find all the explicit zero-initializations and see if any of the
         // generic int shadows can be replaced by real field references.
#if LOCAL_OBJECTS_COLLECTABLE
         // Any zero-initializations for collectable fields are removed.
         // These fields must be zero-initialized at the start of the method
         // instead of at the point of allocation, since liveness is not easily
         // predictable for these objects.
#endif
         //
         for (initTree = candidate->_treeTop->getNextTreeTop(); initTree; initTree = next)
            {
            next = initTree->getNextTreeTop();
            node = initTree->getNode();
            if (node->getOpCodeValue() != TR::istorei ||
                node->getSymbol() != getSymRefTab()->findGenericIntShadowSymbol() ||
                node->getFirstChild() != candidate->_node)
               break;

            int32_t zeroInitOffset = node->getSymbolReference()->getOffset();

            // If this is a zero-initialization for a collectable field, remove
            // it since the initialization will be done at the start of the
            // method.  Don't do this for allocations that are inside loops, since
            // for these allocations the initialization must happen every time
            // round the loop.
            //
            if (referenceSlots && !candidate->isInsideALoop())
               {
               for (j = 0; referenceSlots[j]; j++)
                  {
                  ////if (zeroInitOffset == referenceSlots[j]*_cg->sizeOfJavaPointer())
                  if (zeroInitOffset == referenceSlots[j]*TR::Compiler->om.sizeofReferenceField())
                     {
                     TR::TransformUtil::removeTree(comp(), initTree);
                     break;
                     }
                  }
               if (referenceSlots[j])
                  continue;
               }

            if (candidate->_fields && (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue))
               {
               for (i = candidate->_fields->size()-1; i >= 0; i--)
                  {
                  FieldInfo &field = candidate->_fields->element(i);
                  offset = field._offset;
                  if (field._symRef &&
                      offset == node->getSymbolReference()->getOffset())
                     {
                     node->getSecondChild()->recursivelyDecReferenceCount();
                     sym = field._symRef->getSymbol();
                     TR::DataType type = sym->getDataType();
                     node->setAndIncChild(1, createConst(comp(), node, type, 0));
                     node->setSymbolReference(field._symRef);
                     TR::Node::recreate(node, comp()->il.opCodeForIndirectStore(type));
                     break;
                     }
                  }
               }
            }
         }
      }
   else
      {
      // Zero-initialize all the non-collectable slots, using field symbol
      // references where possible.  For news that are inside loops, also initialize
      // the collectable slots (in addition to their initialization at method entry)
      //
      initTree = candidate->_treeTop;

      if (candidate->_kind == TR::newarray)
         {
         // TODO (Task 118458): We need to investigate the effect of using arrayset on small number of elements. This involves verifying the instruction
         // sequences each individual codegen will generate and only if the codegens can handle arraysets of small number of elements in a performant manner
         // can we enable this optimization.
         const bool enableNewArrayArraySetPendingInvestigation = false;

         // Non-collectable slots should be initialized with arrayset since they do not have field symrefs
         if (enableNewArrayArraySetPendingInvestigation && tryToZeroInitializeUsingArrayset(candidate, initTree))
            {
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/zeroInitializeArrayset/%s/primitive", comp()->signature()), initTree->getNextTreeTop());

            return;
            }
         }

      int32_t headerSize = (candidate->_kind == TR::New || candidate->_kind == TR::newvalue)
                              ? comp()->fej9()->getObjectHeaderSizeInBytes()
                              : TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      int32_t refSlotIndex = 0;
      // Changes for new 64-bit object model
      i = headerSize;
      //for (i = headerSize; i < candidate->_size; i += _cg->sizeOfJavaPointer())
      int32_t refIncrVal = TR::Compiler->om.sizeofReferenceField();////TR::Symbol::convertTypeToSize(TR::Address);
      int32_t intIncrVal = 4; //TR::Symbol::convertTypeToSize(TR_SInt32)

      while (i < candidate->_size)
         {
         if (!candidate->isInsideALoop() && referenceSlots && i == referenceSlots[refSlotIndex]*TR::Compiler->om.sizeofReferenceField())
            {
            refSlotIndex++;
            i += refIncrVal;
            continue;
            }

         // See if the slot can be initialized using a field reference
         //
         if (candidate->_fields && (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue))
            {
            for (j = candidate->_fields->size()-1; j >= 0; j--)
               {
               FieldInfo &field = candidate->_fields->element(j);
               if (field._offset == i && field._symRef)
                  {
                  TR::DataType type = field._symRef->getSymbol()->getDataType();
                  node = createConst(comp(), allocationNode, type, 0);
                  node = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectStore(type), 2, 2, allocationNode, node, field._symRef);
                  initTree = TR::TreeTop::create(comp(), initTree, node);
                  i += field._size;
                  TR_ASSERT(field._size != 0, "assertion failure");
                  break;
                  }
               }
            if (j >= 0)
               continue;
            }

         // If we have no field reference, we have to initialize using a generic int shadow.
         // Since we don't know the type, we have to initialize only a 4byte slot.
         //
         node = TR::Node::create(allocationNode, TR::iconst, 0);
         node = TR::Node::createWithSymRef(TR::istorei, 2, 2, allocationNode, node,
                                (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
                                      ? getSymRefTab()->findOrCreateGenericIntNonArrayShadowSymbolReference(i)
                                      : getSymRefTab()->findOrCreateGenericIntArrayShadowSymbolReference(i));
         initTree = TR::TreeTop::create(comp(), initTree, node);
         i += intIncrVal;
         }
      }

   // Now go through the collectable reference slots and initialize them at the
   // start of the method
   //
   if (referenceSlots)
      {
      initTree = comp()->getStartTree();

      if (tryToZeroInitializeUsingArrayset(candidate, initTree))
         {
         TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/zeroInitializeArrayset/%s/reference", comp()->signature()), initTree->getNextTreeTop());

         return;
         }

      TR::Node *baseNode = NULL;

      for (i = 0; referenceSlots[i]; i++)
         {
         ////int32_t offset = referenceSlots[i] * _cg->sizeOfJavaPointer();
         int32_t offset = referenceSlots[i] * TR::Compiler->om.sizeofReferenceField();

         // See if the slot can be initialized using a field reference
         //
         if (!baseNode)
            {
            baseNode = TR::Node::createWithSymRef(allocationNode, TR::loadaddr, 0, symRef);
            if (candidate->escapesInColdBlocks() || candidate->_seenArrayCopy || candidate->_argToCall || candidate->_seenSelfStore || candidate->_seenStoreToLocalObject)
               {
               if (candidate->escapesInColdBlocks())
                   baseNode->setEscapesInColdBlock(true);
               baseNode->setCannotTrackLocalUses(true);
               if (candidate->callsStringCopyConstructor())
                  baseNode->setCannotTrackLocalStringUses(true);
               }
            }

         if (candidate->_fields && (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue))
            {
            for (j = candidate->_fields->size()-1; j >= 0; j--)
               {
               FieldInfo &field = candidate->_fields->element(j);
               if (field._offset == offset && field._symRef)
                  {
                  TR::DataType type = field._symRef->getSymbol()->getDataType();
                  node = createConst(comp(), allocationNode, type, 0);
                  node = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectStore(type), 2, 2, baseNode, node, field._symRef);
                  if (comp()->useCompressedPointers())
                     initTree = TR::TreeTop::create(comp(), initTree, TR::Node::createCompressedRefsAnchor(node));
                  else
                     initTree = TR::TreeTop::create(comp(), initTree, node);
                  break;
                  }
               }
            if (j >= 0)
               continue;
            }

         // If not, use a generic int shadow to zero-initialize.
         //
         node = TR::Node::aconst(allocationNode, 0);
         //symRef = getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(offset);
         TR::Node *storeNode = TR::Node::createWithSymRef(TR::astorei, 2, 2,baseNode,node,
                                            (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
                                               ? getSymRefTab()->findOrCreateGenericIntNonArrayShadowSymbolReference(offset)
                                               : getSymRefTab()->findOrCreateGenericIntArrayShadowSymbolReference(offset));
         if (comp()->useCompressedPointers())
            initTree = TR::TreeTop::create(comp(), initTree, TR::Node::createCompressedRefsAnchor(storeNode));
         else
            initTree = TR::TreeTop::create(comp(), initTree, storeNode);
         }
      }
   }

void TR_EscapeAnalysis::makeNonContiguousLocalAllocation(Candidate *candidate)
   {
   if (comp()->suppressAllocationInlining())
      return;

   if (comp()->generateArraylets() && (candidate->_kind != TR::New) && (candidate->_kind != TR::newvalue))
      return;

   if (candidate->objectIsReferenced())
      {
      dumpOptDetails(comp(), "%sMaking %s node [%p] into separate local fields and a local object\n",OPT_DETAILS, candidate->_node->getOpCode().getName(), candidate->_node);
      }
   else
      {
      dumpOptDetails(comp(), "%sMaking %s node [%p] into separate local fields\n",OPT_DETAILS, candidate->_node->getOpCode().getName(), candidate->_node);
      }

   if (trace())
      {
      traceMsg(comp(),"Pass: (%d) Non-contiguous allocation found in %s\n", manager()->numPassesCompleted(), comp()->signature());
      //printf("Pass: (%d) Non-contiguous allocation found in %s\n", manager()->numPassesCompleted(), comp()->signature());
      }

   if (candidate->_node->getOpCodeValue() == TR::newvalue)
      {
      makeNewValueLocalAllocation(candidate);
      }
   else if (candidate->_fields)
      {
      // Zero-initialize all the fields
      //
      for (int32_t i = candidate->_fields->size()-1; i >= 0; i--)
         {
         FieldInfo &autoField = candidate->_fields->element(i);
         if (!autoField._symRef || !autoField._symRef->getSymbol()->isAuto())
            continue;

         // If there was explicit zero-initialization of this object, only
         // initialize this field if it intersects the explicit zero-initialization
         //
         if (candidate->isExplicitlyInitialized())
            {
            if (!candidate->_initializedWords)
               continue;
            int32_t j;
            for (j = autoField._size-1; j >= 0; j--)
               {
               if (candidate->_initializedWords->get(autoField._offset+j))
                  break;
               }
            if (j < 0)
               continue;
            }

         TR::DataType type = autoField._symRef->getSymbol()->getDataType();
         TR::Node *node = createConst(comp(), candidate->_node, type, 0);
         node = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(type), 1, 1, node, autoField._symRef);
         TR::TreeTop::create(comp(), candidate->_treeTop, node);

         }
      }

   if (candidate->escapesInColdBlocks())
      {
      // For newvalue operations, heapification should be performed with a newvalue
      // that specifies the field values, so the object can be created correctly
      candidate->_originalAllocationNode = candidate->_node->duplicateTree();
      }

   // If the object was referenced we will need to create a local object for it too - it's
   // used to locate all references to the local object at the point of heapification
   //
   //   - For TR::New and TR::newvalue, a local object of type "java/lang/Object" is created;
   //   - For arrays, a local array of length zero is created
   //
   if (candidate->objectIsReferenced())
      {
      if ((candidate->_kind == TR::New) || (candidate->_kind == TR::newvalue))
         {
         // Change the node so that it allocates a java/lang/Object object
         //
         TR::ResolvedMethodSymbol *owningMethodSymbol = candidate->_node->getSymbolReference()->getOwningMethodSymbol(comp());
         TR_OpaqueClassBlock *classObject = comp()->getObjectClassPointer();

         // the call to findOrCreateClassSymbol is safe even though we pass CPI of -1 it is guarded by another check
         // (see the first occurrence of findOrCreateClassSymbol in EA; dememoizedConstructorCall)
         //
         TR::SymbolReference *classSymRef = getSymRefTab()->findOrCreateClassSymbol(owningMethodSymbol, -1, classObject, false);
         TR::Node *classNode = TR::Node::createWithSymRef(candidate->_node, TR::loadaddr, 0, classSymRef);
         candidate->_node->removeAllChildren();
         candidate->_node->setAndIncChild(0, classNode);
         TR::Node::recreate(candidate->_node, TR::New);
         candidate->_node->setNumChildren(1);
         candidate->_class = classObject;
         candidate->_origSize = candidate->_size;
         candidate->_origKind = candidate->_kind;
         candidate->_size = comp()->fej9()->getObjectHeaderSizeInBytes() + TR::Compiler->cls.classInstanceSize(classObject);
         candidate->_kind = TR::New;
         }
      else
         {
         candidate->_origSize = candidate->_size;
         candidate->_origKind = candidate->_kind;

         // Zero length hybrid arrays have a discontiguous shape.
         //
         candidate->_size = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();

         TR::Node *sizeChild = candidate->_node->getFirstChild();
         TR_ASSERT(sizeChild->getOpCodeValue() == TR::iconst, "The size of non-contiguous stack allocated array object should be constant\n");
         if (sizeChild->getReferenceCount() == 1)
            sizeChild->setInt(0);
         else
            {
            TR::Node *newSizeChild = TR::Node::create(sizeChild, TR::iconst, 0);
            newSizeChild->setInt(0);
            candidate->_node->setAndIncChild(0, newSizeChild);
            sizeChild->decReferenceCount();
            }
         }
      candidate->setExplicitlyInitialized(false);

      makeLocalObject(candidate);
      }

  else
      {
      // Insert debug counter for a noncontiguous allocation.  Counter name is of the form:
      //
      //    escapeAnalysis/noncontiguous-allocation/<hotness>/<outermost-method-sig>/(<inlined-method-sig>)/(<bc-caller-index,bc-offset>)
      //
      // If the node is not from an inlined method, <inlined-method-sig> repeats the outermost method signature
      //
      TR_ByteCodeInfo bcInfo = candidate->_node->getByteCodeInfo();

      const char *dbgCntName = TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/noncontiguous-allocation/%s/%s/(%s)/(%d,%d)",
                                     comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(),
                                     (bcInfo.getCallerIndex() > -1)
                                        ? comp()->getInlinedResolvedMethod(bcInfo.getCallerIndex())->signature(trMemory())
                                        : comp()->signature(),
                                     bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

      TR::DebugCounter::prependDebugCounter(comp(), dbgCntName, candidate->_treeTop);

      // Remove the tree containing the allocation node. All uses of the node
      // should have been removed by now
      //
      TR_ASSERT(candidate->_node->getReferenceCount() == 1, "assertion failure");
      TR::TransformUtil::removeTree(comp(), candidate->_treeTop);
      }
   }



void TR_EscapeAnalysis::heapifyForColdBlocks(Candidate *candidate)
   {
   static char *disableSelectOpForEA = feGetEnv("TR_disableSelectOpForEA");
   bool useSelectOp = !disableSelectOpForEA && cg()->getSupportsSelect();

   if (comp()->suppressAllocationInlining())
      return;

   if (trace())
      {
      traceMsg(comp(),"Found candidate allocated with cold block compensation in %s numBlocks compensated = %d\n", comp()->signature(), candidate->getColdBlockEscapeInfo()->getSize());
      //printf("Found candidate allocated with cold block compensation in %s numBlocks compensated = %d\n", comp()->signature(), candidate->getColdBlockEscapeInfo()->getSize());
      }

   TR::SymbolReference *heapSymRef = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);

   TR::TreeTop *allocationTree = candidate->_treeTop;
   TR::TreeTop *nextTree = allocationTree->getNextTreeTop();
   TR::Node *heapSymRefStore = TR::Node::createWithSymRef(TR::astore, 1, 1, candidate->_node, heapSymRef);
   TR::TreeTop *heapSymRefStoreTree = TR::TreeTop::create(comp(), heapSymRefStore, NULL, NULL);
   allocationTree->join(heapSymRefStoreTree);
   heapSymRefStoreTree->join(nextTree);

   if (candidate->isContiguousAllocation())
      {
      candidate->_node->setCannotTrackLocalUses(true);
      candidate->_node->setEscapesInColdBlock(true);
      if (candidate->callsStringCopyConstructor())
         candidate->_node->setCannotTrackLocalStringUses(true);
      //if (candidate->_originalAllocationNode)
      //   candidate->_originalAllocationNode->setCannotTrackLocalUses(true);
      }

   ListIterator<TR_ColdBlockEscapeInfo> coldBlockInfoIt(candidate->getColdBlockEscapeInfo());
   TR_ColdBlockEscapeInfo *info;
   TR::CFG *cfg = comp()->getFlowGraph();
   for (info = coldBlockInfoIt.getFirst(); info != NULL; info = coldBlockInfoIt.getNext())
      {
      // Invalidate structure if adding blocks; can be repaired probably in this
      // case if needed in the future
      //
      comp()->getFlowGraph()->setStructure(NULL);


      // Create the heap allocation
      //
      TR::Block *coldBlock = info->getBlock();
      TR::TreeTop *insertionPoint = coldBlock->getEntry();
      TR::TreeTop *treeBeforeInsertionPoint = insertionPoint->getPrevTreeTop();
      TR::Node *heapAllocation = TR::Node::create(TR::treetop, 1, candidate->_originalAllocationNode->duplicateTree());
      heapAllocation->getFirstChild()->setHeapificationAlloc(true);

      if (trace())
         traceMsg(comp(), "heapifying %p:  isContiguousAllocation %d; _dememoizedMethodSymRef %d; new heapAllocation treetop %p\n", candidate->_node, candidate->isContiguousAllocation(), candidate->_dememoizedMethodSymRef, heapAllocation);

      if (!candidate->isContiguousAllocation() && _dememoizedAllocs.find(candidate->_node))
         {
         heapAllocation->getFirstChild()->getFirstChild()->recursivelyDecReferenceCount(); // remove loadaddr of class

         TR::SymbolReference *autoSymRefForValue = NULL;
         if (candidate->_fields)
            {
            int32_t j;
            int32_t fieldSize = 0;
            for (j = candidate->_fields->size()-1; j >= 0; j--)
               {
               FieldInfo &field = candidate->_fields->element(j);
               fieldSize = field._size;

               // TODO-VALUETYPE:  Need to handle dememoized Integer.valueOf if Integer becomes a value type
               if (field._symRef &&
                   field._symRef->getSymbol()->isAuto() &&
                   (candidate->_origKind == TR::New))
                  {
                  autoSymRefForValue = field._symRef;
                  break;
                  }
               }
            }

         TR::Node *stackFieldLoad = NULL;
         if (autoSymRefForValue)
            stackFieldLoad = TR::Node::createWithSymRef(heapAllocation, comp()->il.opCodeForDirectLoad(autoSymRefForValue->getSymbol()->getDataType()), 0, autoSymRefForValue);
         else
            stackFieldLoad = TR::Node::create(heapAllocation, TR::iconst, 0, 0);

         heapAllocation->getFirstChild()->setAndIncChild(0, stackFieldLoad);
         TR::Node::recreate(heapAllocation->getFirstChild(), TR::acall);
         heapAllocation->getFirstChild()->setSymbolReference(_dememoizationSymRef);
         }



      TR::TreeTop *heapAllocationTree = TR::TreeTop::create(comp(), heapAllocation, NULL, NULL);
      TR::Node *heapStore = TR::Node::createWithSymRef(TR::astore, 1, 1, heapAllocation->getFirstChild(), heapSymRef);
      TR::TreeTop *heapStoreTree = TR::TreeTop::create(comp(), heapStore, NULL, NULL);
      TR::Block *heapAllocationBlock = toBlock(cfg->addNode(TR::Block::createEmptyBlock(heapAllocation, comp(), coldBlock->getFrequency())));
      heapAllocationBlock->inheritBlockInfo(coldBlock, coldBlock->isCold());


      // Check if a heap object has been created for this stack allocated
      // candidate before
      //
      TR::Node *tempLoad = TR::Node::createWithSymRef(heapAllocation, comp()->il.opCodeForDirectLoad(TR::Address), 0, heapSymRef);
      TR::Node *heapComparisonNode = TR::Node::createif(TR::ifacmpne, tempLoad, candidate->_node->duplicateTree(), NULL);
      TR::TreeTop *heapComparisonTree = TR::TreeTop::create(comp(), heapComparisonNode, NULL, NULL);
      TR::Block *heapComparisonBlock = toBlock(cfg->addNode(TR::Block::createEmptyBlock(heapComparisonNode, comp(), coldBlock->getFrequency())));
      heapComparisonBlock->inheritBlockInfo(coldBlock, coldBlock->isCold());

      TR::TreeTop *heapComparisonEntryTree = heapComparisonBlock->getEntry();
      TR::TreeTop *heapComparisonExitTree = heapComparisonBlock->getExit();
      heapComparisonEntryTree->join(heapComparisonTree);
      heapComparisonTree->join(heapComparisonExitTree);

      heapComparisonExitTree->join(insertionPoint);

      if (treeBeforeInsertionPoint)
        treeBeforeInsertionPoint->join(heapComparisonEntryTree);
      else
        comp()->setStartTree(heapComparisonEntryTree);

      treeBeforeInsertionPoint = heapComparisonExitTree;

      //cfg->addEdge(heapComparisonBlock, firstComparisonBlock);
      cfg->addEdge(heapComparisonBlock, heapAllocationBlock);

      // Copy the contents into newly created heap allocation
      //
      TR::TreeTop *heapAllocationEntryTree = heapAllocationBlock->getEntry();
      TR::TreeTop *heapAllocationExitTree = heapAllocationBlock->getExit();
      heapAllocationEntryTree->join(heapAllocationTree);
      heapAllocationTree->join(heapStoreTree);
      heapStoreTree->join(heapAllocationExitTree);

      TR_ByteCodeInfo bcInfo = candidate->_node->getByteCodeInfo();

      const char *dbgCntName = TR::DebugCounter::debugCounterName(comp(), "escapeAnalysis/heapification/%s/%s/(%s)/(%d,%d)",
                                     comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(),
                                     (bcInfo.getCallerIndex() > -1)
                                        ? comp()->getInlinedResolvedMethod(bcInfo.getCallerIndex())->signature(trMemory())
                                        : comp()->signature(),
                                     bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

      TR::DebugCounter::prependDebugCounter(comp(), dbgCntName, heapAllocationTree);

      heapAllocationExitTree->join(insertionPoint);

      if (treeBeforeInsertionPoint)
         treeBeforeInsertionPoint->join(heapAllocationEntryTree);
      else
         comp()->setStartTree(heapAllocationEntryTree);

      treeBeforeInsertionPoint = heapStoreTree;
      TR::Node *stackAllocation = candidate->_node->duplicateTree();

      if (candidate->_origKind != TR::newvalue)
         {
         // Copy all the slots, using field symbol references where possible.
         //
         int32_t headerSize = (candidate->_origKind == TR::New) ?
            comp()->fej9()->getObjectHeaderSizeInBytes() :
            TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         int32_t i;
         int32_t size = candidate->_size;
         if (!candidate->isContiguousAllocation())
            size = candidate->_origSize;
         // Changes for new 64-bit object model
         int32_t refIncrVal = TR::Symbol::convertTypeToSize(TR::Address);
         int32_t intIncrVal = 4; // TR::Symbol::convertTypeToSize(TR_SInt32);
         i = headerSize;
         //for (i = headerSize; i < size; i += _cg->sizeOfJavaPointer())
         while (i < size)
            {
            //
            // See if the slot can be initialized using a field reference
            //
            if (candidate->_fields)
               {
               int32_t j;
               int32_t fieldSize = 0;
               for (j = candidate->_fields->size()-1; j >= 0; j--)
                  {
                  FieldInfo &field = candidate->_fields->element(j);
                  fieldSize = field._size;
                  if (field._offset == i &&
                      field._symRef &&
                      (candidate->isContiguousAllocation() || field._symRef->getSymbol()->isAuto()) &&
                      (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue))
                     {
                     TR::DataType type = field._symRef->getSymbol()->getDataType();

                     TR::Node *stackFieldLoad = NULL;
                     if (!candidate->isContiguousAllocation())
                        stackFieldLoad = TR::Node::createWithSymRef(heapAllocation, comp()->il.opCodeForDirectLoad(type), 0, field._symRef);
                     else
                        stackFieldLoad = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(type), 1, 1, stackAllocation, field._symRef);

                     TR::Node *heapFieldStore = NULL;
                     TR::TreeTop *translateTT = NULL;
                     if (stackFieldLoad->getDataType() == TR::Address)
                        {
                        heapFieldStore = TR::Node::createWithSymRef(TR::awrtbari, 3, 3, heapAllocation->getFirstChild(), stackFieldLoad, heapAllocation->getFirstChild(), field.fieldSymRef());
                        if (comp()->useCompressedPointers())
                           {
                           translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(heapFieldStore), NULL, NULL);
                           }
                        }
                     else
                        heapFieldStore = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectStore(type), 2, 2, heapAllocation->getFirstChild(), stackFieldLoad, field.fieldSymRef());
                     TR::TreeTop *heapFieldStoreTree = NULL;
                     //comp()->useCompressedPointers()
                     if (translateTT)
                        heapFieldStoreTree = translateTT;
                     else
                        heapFieldStoreTree = TR::TreeTop::create(comp(), heapFieldStore, NULL, NULL);
                     treeBeforeInsertionPoint->join(heapFieldStoreTree);
                     heapFieldStoreTree->join(heapAllocationExitTree);
                     treeBeforeInsertionPoint = heapFieldStoreTree;
                     break;
                     }
                  }
               if (j >= 0)
                  {
                  i += fieldSize;
                  continue;
                  }
               }

            // If not, use a generic int shadow to initialize.
            //
            // don't exceed the object size
//             if ((i + refIncrVal) <= size)
//                {
//                TR::SymbolReference *intShadow = getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(i);

//                TR::Node *stackFieldLoad = NULL;
//                if (candidate->isContiguousAllocation())
//                   stackFieldLoad = TR::Node::create(TR::aloadi, 1, stackAllocation, intShadow);
//                else
//                   stackFieldLoad = TR::Node::aconst(heapAllocation, 0);

//                TR::Node *heapFieldStore = TR::Node::create(TR::astorei, 2, heapAllocation->getFirstChild(), stackFieldLoad, intShadow);
//                TR::TreeTop *heapFieldStoreTree = TR::TreeTop::create(comp(), heapFieldStore, NULL, NULL);
//                treeBeforeInsertionPoint->join(heapFieldStoreTree);
//                heapFieldStoreTree->join(heapAllocationExitTree);
//                treeBeforeInsertionPoint = heapFieldStoreTree;
//                i += refIncrVal;
//                }
//             else
//                {
               TR::SymbolReference *intShadow;
               if (candidate->_origKind == TR::New || candidate->_origKind == TR::newvalue)
                  intShadow = getSymRefTab()->findOrCreateGenericIntNonArrayShadowSymbolReference(i);
               else
                  intShadow = getSymRefTab()->findOrCreateGenericIntArrayShadowSymbolReference(i);

               TR::Node *stackFieldLoad = NULL;
               if (candidate->isContiguousAllocation())
                  stackFieldLoad = TR::Node::createWithSymRef(TR::iloadi, 1, 1, stackAllocation, intShadow);
               else
                  stackFieldLoad = TR::Node::create(heapAllocation, TR::iconst, 0);
               TR::Node *heapFieldStore = TR::Node::createWithSymRef(TR::istorei, 2, 2, heapAllocation->getFirstChild(), stackFieldLoad, intShadow);
               TR::TreeTop *heapFieldStoreTree = TR::TreeTop::create(comp(), heapFieldStore, NULL, NULL);
               treeBeforeInsertionPoint->join(heapFieldStoreTree);
               heapFieldStoreTree->join(heapAllocationExitTree);
               treeBeforeInsertionPoint = heapFieldStoreTree;
               i += intIncrVal;
//                }
            }
         }
      else // if (candidate->_origKind == TR::newvalue)
         {
         // candidate->_class will be set to java/lang/Object for non-contiguous stack allocation
         // of value type instances that are referenced, so we must rely on the candidate->_origClass
         // field to get the actual value class for heapification
         //
         TR_OpaqueClassBlock *valueClass = (TR_OpaqueClassBlock*)candidate->_origClass;
         const TR::TypeLayout* typeLayout = comp()->typeLayout(valueClass);
         size_t fieldCount = typeLayout->count();

         TR_ASSERT_FATAL(candidate->_fields != NULL || fieldCount == 0, "candidate %p of kind newvalue is expected to have non-NULL candidate->_fields if it has a non-zero fieldCount %d\n", candidate->_node, fieldCount);

         TR::Node *newValueHeapificationNode = heapAllocation->getFirstChild();

         // Generate IL to recreate newvalue on heap using values from stack allocated fields.
         // For a contiguous stack allocated instance, this might look like the following
         // where field values are loaded from the stack allocated aggregate.
         //
         //   n164n     treetop
         //   n160n       newvalue  jitNewValue
         //   n161n         loadaddr  Pair
         //   n178n         iloadi  Pair.x I[#425  final Shadow +4]
         //   n177n           loadaddr  <temp slot 6>[#439  Auto]
         //   n179n         iloadi  Pair.y I[#427  final Shadow +8]
         //   n177n           ==>loadaddr
         //
         // For a non-contiguous stack allocated instance, this might look like the following
         // where field values are loaded from the individual temporaries for each field.
         //
         //   n181n     treetop
         //   n177n       newvalue  jitNewValue
         //   n178n         loadaddr  Pair
         //   n179n         iload  <temp slot 8>[#440  Auto]
         //   n180n         iload  <temp slot 8>[#441  Auto]


         if (trace())
            {
            traceMsg(comp(), "Updating heap allocation node %p with fieldCount == %d\n", newValueHeapificationNode, fieldCount);
            }

         for (size_t idx = 0; idx < fieldCount; idx++)
            {
            const TR::TypeLayoutEntry &fieldEntry = typeLayout->entry(idx);

            TR::Node *stackFieldLoad = NULL;
            int32_t j;

            for (j = candidate->_fields->size()-1; j >= 0; j--)
               {
               FieldInfo &field = candidate->_fields->element(j);

               if (field._offset == fieldEntry._offset &&
                   field._symRef &&
                   (candidate->isContiguousAllocation() || field._symRef->getSymbol()->isAuto()))
                  {
                  TR::DataType type = field._symRef->getSymbol()->getDataType();

                  if (!candidate->isContiguousAllocation())
                     {
                     stackFieldLoad = TR::Node::createWithSymRef(newValueHeapificationNode, comp()->il.opCodeForDirectLoad(type), 0, field._symRef);
                     }
                  else
                     {
                     stackFieldLoad = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(type), 1, 1, stackAllocation, field._symRef);
                     }
                  break;
                  }
               }

            TR_ASSERT_FATAL(stackFieldLoad != NULL, "newvalue candidate %p was missing an entry in candidate->_fields for its field number %d at offset %d", candidate->_node, idx, fieldEntry._offset);
            newValueHeapificationNode->setAndIncChild(idx+1, stackFieldLoad);
            if (trace())
               {
               traceMsg(comp(), "Updating heap allocation node %p child %d with field load node %p\n", newValueHeapificationNode, idx+1, stackFieldLoad);
               }
            }
         }

      insertionPoint = coldBlock->getEntry();
      treeBeforeInsertionPoint = insertionPoint->getPrevTreeTop();
      TR::Block *targetBlock = coldBlock;
      ListIterator<TR::Node> nodesIt(info->getNodes());
      ListIterator<TR::TreeTop> treesIt(info->getTrees());
      TR::Node *escapeNode;
      TR::TreeTop *escapeTree = treesIt.getFirst();
      TR::Block *lastComparisonBlock = NULL, *lastStoreBlock = NULL;
      for (escapeNode = nodesIt.getFirst(); escapeNode != NULL; escapeNode = nodesIt.getNext(), escapeTree = treesIt.getNext())
        {
        bool skipStores = false;
        if (escapeTree->getNode()->getOpCode().isReturn())
           skipStores = true;
        else if (escapeTree->getNode()->getOpCodeValue() == TR::treetop ||
                escapeTree->getNode()->getOpCode().isNullCheck())
           {
           TR::Node *firstChild = escapeTree->getNode()->getFirstChild();
           if (firstChild->getOpCodeValue() == TR::athrow)
              skipStores = true;
           }

        if ((!candidate->isContiguousAllocation()) && !skipStores && !isImmutableObject(candidate) )
            {
            //
            // Store back into all the temp slots, using field symbol references where possible.
            //

            //TR::TreeTop *coldBlockTree = coldBlock->getLastRealTreeTop();
            //TR::Node *coldBlockNode = coldBlockTree->getNode();

            //if (coldBlockNode->getOpCodeValue() == TR::treetop)
            //   coldBlockNode = coldBlockNode->getFirstChild();

            //if (coldBlockNode->getOpCode().isBranch() ||
            //        coldBlockNode->getOpCode().isSwitch() ||
            //        coldBlockNode->getOpCode().isReturn() ||
            //        coldBlockNode->getOpCodeValue() == TR::athrow)
            //  coldBlockTree = coldBlockTree->getPrevTreeTop();
            //
            TR::TreeTop *coldBlockTree = escapeTree;

            int32_t headerSize = ((candidate->_origKind == TR::New) || (candidate->_origKind == TR::newvalue))
                                 ? comp()->fej9()->getObjectHeaderSizeInBytes()
                                 : TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
            int32_t size = candidate->_origSize;
            // Changes for new 64-bit object model
            // instead of _cg->sizeOfJavaPointer(), increment by field size
            //for (i = headerSize; i < size; i += incrVal)
            int32_t i = headerSize;
            int32_t incrVal = 4; // TR::Symbol::convertTypeToSize(TR_SInt32)
            while (i < size)
               {
               //
               // See if the slot can be initialized using a field reference
               //
               TR::TreeTop *nextTreeInColdBlock = coldBlockTree->getNextTreeTop();
               if (candidate->_fields)
                  {
                  int32_t j;
                  for (j = candidate->_fields->size()-1; j >= 0; j--)
                     {
                     FieldInfo &field = candidate->_fields->element(j);
                     if (field._offset == i &&
                         field._symRef && field._symRef->getSymbol()->isAuto() /* &&
                         field._size == TR::Symbol::convertTypeToSize(TR_SInt32)*/)
                        {
                        TR::DataType type = field._symRef->getSymbol()->getDataType();

                        TR::Node *heapFieldLoad = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(type), 1, 1, escapeNode, field.fieldSymRef());
                        TR::TreeTop *translateTT = NULL;
                        if (comp()->useCompressedPointers()
                              && (type == TR::Address))
                           {
                           translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(heapFieldLoad), NULL, NULL);
                           }
                        TR::Node *stackStore = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(type), 1, 1, heapFieldLoad, field._symRef);
                        TR::TreeTop *stackStoreTree = TR::TreeTop::create(comp(), stackStore, NULL, NULL);
                        if (trace())
                           traceMsg(comp(), "Emitting stack store back %p cold %p next %p\n", stackStore, coldBlockTree->getNode(), nextTreeInColdBlock->getNode());
                        coldBlockTree->join(stackStoreTree);
                        stackStoreTree->join(nextTreeInColdBlock);
                        // comp()->useCompressedPointers()
                        if (translateTT)
                           {
                           coldBlockTree->join(translateTT);
                           translateTT->join(stackStoreTree);
                           }
                        coldBlockTree = stackStoreTree;
                        // increment by data type size
                        i += field._size;
                        break;
                        }
                     }
                  if (j >= 0)
                     continue;
                  }
               i += incrVal;
               }
            }
        }

      TR::TreeTop *insertSymRefStoresAfter = NULL;

      // If using aselect to perform comparisons, all compares and stores are
      // inserted directly at the start of the cold block
      if (useSelectOp)
         {
         insertSymRefStoresAfter = coldBlock->getEntry();
         }

      ListIterator<TR::SymbolReference> symRefsIt(candidate->getSymRefs());
      TR::SymbolReference *symRef;
      bool generatedReusedOperations = false;
      TR::Node *heapTempLoad = NULL;
      TR::Node *candidateStackAddrLoad = NULL;

      for (symRef = symRefsIt.getFirst(); symRef; symRef = symRefsIt.getNext())
        {
        //
        // Now create the compares (one for each node) and stores
        //
        if (useSelectOp)
           {
           // Reload address of object on heap just once for this block
           if (!heapTempLoad)
              {
              heapTempLoad = TR::Node::createWithSymRef(candidate->_node, TR::aload, 0, heapSymRef);
              candidateStackAddrLoad = candidate->_node->duplicateTree();
              }

           // If variable has address of the stack allocated object, replace
           // with the value of the heap allocated object; otherwise, keep the
           // current value
           //
           // astore <object-temp>
           //   aselect
           //     acmpeq
           //       aload <object-temp>
           //       loadaddr <stack-obj>
           //     aload <heap-allocated-obj>
           //     aload <object-temp>
           //
           TR::Node *symLoad = TR::Node::createWithSymRef(candidate->_node, TR::aload, 0, symRef);
           TR::Node *addrCompareNode = TR::Node::create(candidate->_node, TR::acmpeq, 2, symLoad, candidateStackAddrLoad);
           TR::Node *chooseAddrNode = TR::Node::create(TR::aselect, 3, addrCompareNode, heapTempLoad, symLoad);

           TR::TreeTop *storeTree = storeHeapifiedToTemp(candidate, chooseAddrNode, symRef);

           storeTree->join(insertSymRefStoresAfter->getNextTreeTop());
           insertSymRefStoresAfter->join(storeTree);
           }
        else
           {
           TR::Node *comparisonNode = TR::Node::createif(TR::ifacmpne, TR::Node::createWithSymRef(candidate->_node, TR::aload, 0, symRef), candidate->_node->duplicateTree(), targetBlock->getEntry());
           TR::TreeTop *comparisonTree = TR::TreeTop::create(comp(), comparisonNode, NULL, NULL);
           TR::Block *comparisonBlock = toBlock(cfg->addNode(TR::Block::createEmptyBlock(comparisonNode, comp(), coldBlock->getFrequency())));
           comparisonBlock->inheritBlockInfo(coldBlock, coldBlock->isCold());

           TR::TreeTop *comparisonEntryTree = comparisonBlock->getEntry();
           TR::TreeTop *comparisonExitTree = comparisonBlock->getExit();
           comparisonEntryTree->join(comparisonTree);
           comparisonTree->join(comparisonExitTree);

           comparisonExitTree->join(insertionPoint);

           if (treeBeforeInsertionPoint)
              treeBeforeInsertionPoint->join(comparisonEntryTree);
           else
              comp()->setStartTree(comparisonEntryTree);

           TR::Node *heapifiedObjAddrLoad = TR::Node::createWithSymRef(comparisonNode, TR::aload, 0, heapSymRef);

           TR::TreeTop *storeTree = storeHeapifiedToTemp(candidate, heapifiedObjAddrLoad, symRef);

           TR::Block *storeBlock = toBlock(cfg->addNode(TR::Block::createEmptyBlock(storeTree->getNode(), comp(), coldBlock->getFrequency())));
           storeBlock->inheritBlockInfo(coldBlock, coldBlock->isCold());

           cfg->addEdge(comparisonBlock, storeBlock);
           cfg->addEdge(comparisonBlock, targetBlock);
           cfg->addEdge(storeBlock, targetBlock);
           if (targetBlock == coldBlock)
             {
             lastComparisonBlock = comparisonBlock;
             lastStoreBlock = storeBlock;
             }

           TR::TreeTop *storeEntryTree = storeBlock->getEntry();
           TR::TreeTop *storeExitTree = storeBlock->getExit();

           comparisonExitTree->join(storeEntryTree);
           storeEntryTree->join(storeTree);
           storeTree->join(storeExitTree);
           storeExitTree->join(insertionPoint);

           insertionPoint = comparisonEntryTree;
           treeBeforeInsertionPoint = insertionPoint->getPrevTreeTop();
           targetBlock = comparisonBlock;
           }
        }

      cfg->addEdge(heapAllocationBlock, targetBlock);
      cfg->addEdge(heapComparisonBlock, targetBlock);
      heapComparisonNode->setBranchDestination(targetBlock->getEntry());

      for (auto pred = coldBlock->getPredecessors().begin(); pred != coldBlock->getPredecessors().end();)
         {
         TR::CFGNode *predNode = (*pred)->getFrom();
         /* might be removed, keep reference to next object in list */
         pred++;
         if ((useSelectOp && (predNode != heapComparisonBlock)
                 && (predNode != heapAllocationBlock))
             || (!useSelectOp && (predNode != lastComparisonBlock)
                 && (predNode != lastStoreBlock))
             || coldBlock->isCatchBlock())
            {
            TR::Block *predBlock = toBlock(predNode);
            if (!coldBlock->isCatchBlock() &&
                (predBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch() ||
                 predBlock->getLastRealTreeTop()->getNode()->getOpCode().isSwitch()))
               {
               TR::TreeTop *lastTree = predBlock->getLastRealTreeTop();
               TR::Node *lastNode = lastTree->getNode();
               if (lastNode->getOpCode().isBranch() &&
                   (lastNode->getBranchDestination() == coldBlock->getEntry()))
                  predBlock->changeBranchDestination(heapComparisonBlock->getEntry(), cfg);
               else
                  {
                  if (lastNode->getOpCode().isSwitch())
                     lastTree->adjustBranchOrSwitchTreeTop(comp(), coldBlock->getEntry(), heapComparisonBlock->getEntry());

                  cfg->addEdge(predNode, heapComparisonBlock);
                  cfg->removeEdge(predNode, coldBlock);
                  }
               }
            else
               {
               cfg->addEdge(predNode, heapComparisonBlock);
               cfg->removeEdge(predNode, coldBlock);
               }
            }
         }

      if (coldBlock->isCatchBlock())
         {
         TR::TreeTop *coldBlockEntry = coldBlock->getEntry();
         TR::TreeTop *coldBlockExit = coldBlock->getExit();
         coldBlock->setEntry(heapComparisonEntryTree);
         coldBlock->setExit(heapComparisonExitTree);
         heapComparisonBlock->setEntry(coldBlockEntry);
         heapComparisonBlock->setExit(coldBlockExit);
         heapComparisonEntryTree->getNode()->setBlock(coldBlock);
         heapComparisonExitTree->getNode()->setBlock(coldBlock);
         coldBlockEntry->getNode()->setBlock(heapComparisonBlock);
         coldBlockExit->getNode()->setBlock(heapComparisonBlock);

         TR_ScratchList<TR::CFGEdge> coldSuccessors(trMemory()), coldExceptionSuccessors(trMemory());

         for (auto succ = coldBlock->getSuccessors().begin(); succ != coldBlock->getSuccessors().end(); ++succ)
            coldSuccessors.add(*succ);

         for (auto succ = coldBlock->getExceptionSuccessors().begin(); succ != coldBlock->getExceptionSuccessors().end(); ++succ)
            coldExceptionSuccessors.add(*succ);

         for (auto succ = heapComparisonBlock->getSuccessors().begin(); succ != heapComparisonBlock->getSuccessors().end();)
            {
            if (!coldBlock->hasSuccessor((*succ)->getTo()))
               cfg->addEdge(coldBlock, (*succ)->getTo());
            cfg->removeEdge(heapComparisonBlock, (*(succ++))->getTo());
            }

         for (auto succ = heapComparisonBlock->getExceptionSuccessors().begin(); succ != heapComparisonBlock->getExceptionSuccessors().end();)
            {
            if (!coldBlock->hasExceptionSuccessor((*succ)->getTo()))
               cfg->addExceptionEdge(coldBlock, (*succ)->getTo());
            cfg->removeEdge(heapComparisonBlock, (*(succ++))->getTo());
            }
         ListIterator<TR::CFGEdge> bi;
         bi.set(&(coldSuccessors));
         for (TR::CFGEdge* succ = bi.getFirst(); succ != NULL; succ = bi.getNext())
            {
            if (!heapComparisonBlock->hasSuccessor(succ->getTo()))
               cfg->addEdge(heapComparisonBlock, succ->getTo());
            cfg->removeEdge(coldBlock, succ->getTo());
            }

         bi.set(&(coldExceptionSuccessors));
         for (TR::CFGEdge* succ = bi.getFirst(); succ != NULL; succ = bi.getNext())
            {
            if (!heapComparisonBlock->hasExceptionSuccessor(succ->getTo()))
               cfg->addExceptionEdge(heapComparisonBlock, succ->getTo());
            cfg->removeEdge(coldBlock, succ->getTo());
            }

         TR::TreeTop *firstTreeTop = coldBlockEntry->getNextTreeTop();
         TR::Node *firstTree = firstTreeTop->getNode();
         while (firstTree->getOpCodeValue() == TR::allocationFence)
            {
            firstTreeTop = firstTreeTop->getNextTreeTop();
            firstTree = firstTreeTop->getNode();
            }

         bool recognizedCatch = true;
         if (!firstTree->getOpCode().isStoreDirect() ||
             !firstTree->getSymbol()->isAuto() ||
             !firstTree->getFirstChild()->getOpCode().hasSymbolReference() ||
             (firstTree->getFirstChild()->getSymbolReference() != comp()->getSymRefTab()->findOrCreateExcpSymbolRef()))
            recognizedCatch = false;

         TR_ASSERT(recognizedCatch, "Catch block is not in recognized form for heapification");

         if (recognizedCatch)
            {
            TR::Node *firstChild = firstTree->getFirstChild();
            TR::TreeTop *excTree = firstTreeTop;
            TR::TreeTop *prevExcTree = excTree->getPrevTreeTop();
            TR::TreeTop *nextExcTree = excTree->getNextTreeTop();
            TR::TreeTop *changedPrevExcTree = heapComparisonEntryTree;
            TR::TreeTop *changedNextExcTree = heapComparisonEntryTree->getNextTreeTop();


            TR::Node *dupFirstChild = firstChild->duplicateTree();
            firstTree->setAndIncChild(0, dupFirstChild);
            firstChild->setSymbolReference(firstTree->getSymbolReference());
            firstChild->decReferenceCount();

            prevExcTree->join(nextExcTree);
            changedPrevExcTree->join(excTree);
            excTree->join(changedNextExcTree);
            }
         }
      }
   }


TR::TreeTop *TR_EscapeAnalysis::storeHeapifiedToTemp(Candidate *candidate, TR::Node *value, TR::SymbolReference *symRef)
   {
   TR::Node *storeNode = TR::Node::createWithSymRef(TR::astore, 1, 1, value, symRef);
   TR::TreeTop *storeTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);

   if (symRef->getSymbol()->holdsMonitoredObject())
      {
      storeNode->setLiveMonitorInitStore(true);
      }
   storeNode->setHeapificationStore(true);

   if (!symRef->getSymbol()->isParm() && !_initializedHeapifiedTemps->get(symRef->getReferenceNumber()))
      {
      TR::Node *initStoreNode = TR::Node::createWithSymRef(TR::astore, 1, 1, TR::Node::aconst(candidate->_node, 0), symRef);
      if (symRef->getSymbol()->holdsMonitoredObject())
         initStoreNode->setLiveMonitorInitStore(true);
      TR::TreeTop *initStoreTree = TR::TreeTop::create(comp(), initStoreNode, NULL, NULL);
      TR::TreeTop *startTree = comp()->getStartTree();
      TR::TreeTop *nextToStart = startTree->getNextTreeTop();
           startTree->join(initStoreTree);
      initStoreTree->join(nextToStart);
      _initializedHeapifiedTemps->set(symRef->getReferenceNumber());
      }

   return storeTree;
   }


bool TR_EscapeAnalysis::devirtualizeCallSites()
   {
   bool devirtualizedSomething = false;
   while (!_devirtualizedCallSites.isEmpty())
      {
      TR::TreeTop *callSite   = _devirtualizedCallSites.popHead();

      devirtualizedSomething = true;

      TR::Node *callNode = callSite->getNode();
      if (callNode->getOpCode().isCheck() || callNode->getOpCodeValue() == TR::treetop)
         callNode = callNode->getFirstChild();
      TR::ResolvedMethodSymbol *calledMethod = callNode->getSymbol()->getResolvedMethodSymbol();
      if (calledMethod && (!(calledMethod->getResolvedMethod()->virtualMethodIsOverridden()) && callNode->getOpCode().isIndirect()))
         {
         TR::Block *block = callSite->getEnclosingBlock();
         TR::Node *guardNode = TR_VirtualGuard::createNonoverriddenGuard(TR_NonoverriddenGuard, comp(),
            callNode->getByteCodeInfo().getCallerIndex(),
            callNode,
            NULL,
            callNode->getSymbol()->getResolvedMethodSymbol(),false);
         dumpOptDetails(comp(), "new guard=%p added for callsite =%p (%p)\n",guardNode,callSite,callNode);
         //create empty tree and let the splitter fix the callNode and then duplicate it.
         TR::TreeTop *compareTree =  TR::TreeTop::create(comp(), guardNode);
         TR::TreeTop *directCallTree = TR::TreeTop::create(comp());
         TR::TreeTop *coldTree = TR::TreeTop::create(comp());
         TR::Block * remainder = block->createConditionalBlocksBeforeTree(callSite, compareTree, coldTree, directCallTree, comp()->getFlowGraph(),false);

         TR::Node * directCall = callNode->duplicateTree();

         TR::Node * directCallTreeNode;

         if (callSite->getNode()->getOpCode().hasSymbolReference())
            directCallTreeNode = TR::Node::createWithSymRef(callSite->getNode()->getOpCodeValue(), 1, 1, directCall, callSite->getNode()->getSymbolReference());
         else
            directCallTreeNode = TR::Node::create(callSite->getNode()->getOpCodeValue(), 1, directCall);

         directCallTree->setNode(directCallTreeNode);

         directCall->devirtualizeCall(directCallTree, comp());


         TR::Node * coldCall = callNode->duplicateTree();

         TR::Node * coldTreeNode;
         if (callSite->getNode()->getOpCode().hasSymbolReference())
            coldTreeNode = TR::Node::createWithSymRef(callSite->getNode()->getOpCodeValue(), 1, 1, coldCall, callSite->getNode()->getSymbolReference());
         else
            coldTreeNode = TR::Node::create(callSite->getNode()->getOpCodeValue(), 1, coldCall);
         coldTree->setNode(coldTreeNode);

         if (callNode->getReferenceCount() >= 1)
            {
            //need to fixup references to the original call
            //store return value to temp (after direct call and after cold call)
            //load it back (instead of the references)

            TR::DataType dt = callNode->getDataType();

            TR::SymbolReference * temp1 = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dt);

            TR::TreeTop *newStoreTree1 = TR::TreeTop::create(comp(), TR::Node::createStore(temp1, directCall));

            newStoreTree1->join(directCallTree->getNextTreeTop());
            directCallTree->join(newStoreTree1);

            //add store of return val after cold call
            TR::TreeTop *newStoreTree2 = TR::TreeTop::create(comp(), TR::Node::createStore(temp1, coldCall));
            newStoreTree2->join(coldTree->getNextTreeTop());
            coldTree->join(newStoreTree2);

            //replace all references of the orig call in the remainder with load of return val
            callNode->removeAllChildren();
            TR::Node::recreate(callNode, comp()->il.opCodeForDirectLoad(dt));
            callNode->setNumChildren(0);
            callNode->setSymbolReference(temp1);
            }
         }
      }

   return devirtualizedSomething;
   }





bool TR_EscapeAnalysis::inlineCallSites()
   {
   scanForExtraCallsToInline();

   bool inlinedSomething = false;
   while (!_inlineCallSites.isEmpty())
      {
      TR::TreeTop              *treeTop   = _inlineCallSites.popHead();
      TR::ResolvedMethodSymbol *methodSym = treeTop->getNode()->getFirstChild()->getSymbol()->getResolvedMethodSymbol();
      TR_ResolvedMethod     *method    = methodSym->getResolvedMethod();
      int32_t                  size      = method->maxBytecodeIndex();

      //The inliner might remove unreachable regions/blocks - check if the remaining calls to inline exist in the remaining trees.
      TR::TreeTop *entryTree = comp()->getStartTree();
      TR::TreeTop *exitTree = comp()->getMethodSymbol()->getLastTreeTop();
      TR::TreeTop *tt;
      for (tt = entryTree->getNextTreeTop(); tt != exitTree; tt = tt->getNextTreeTop())
         {
         if ((tt->getNode()->getNumChildren() > 0) &&
            tt->getNode()->getFirstChild() == treeTop->getNode()->getFirstChild())
         break;
         }
      if (tt == exitTree)
         {
         if (trace())
            traceMsg(comp(), "attempt to inline call %p failed because the block was removed\n",treeTop->getNode()->getFirstChild());
         continue;
         }

      if (!alwaysWorthInlining(treeTop->getNode()->getFirstChild()))
         {
         // Check size thresholds so we don't inline the universe
         //
         if (getOptData()->_totalInlinedBytecodeSize + size > _maxInlinedBytecodeSize)
            {
            dumpOptDetails(comp(), "\nNOT inlining method %s into treetop at [%p], total inlined size = %d\n", method->signature(trMemory()), treeTop->getNode(), getOptData()->_totalInlinedBytecodeSize + size);
            return false;
            }
         }

      if (trace())
         {
         /////printf("secs Inlining method %s in %s\n", method->signature(trMemory()), comp()->signature());
         traceMsg(comp(), "\nInlining method %s into treetop at [%p], total inlined size = %d\n", method->signature(trMemory()), treeTop->getNode(), getOptData()->_totalInlinedBytecodeSize+size);
         }

      // Now inline the call
      //

      bool toInlineFully = (treeTop->getNode()->getFirstChild()->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_lang_Integer_init) || (treeTop->getNode()->getFirstChild()->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_lang_Integer_valueOf);
      if (performTransformation(comp(), "%sAttempting to inline call [%p]%s\n", OPT_DETAILS, treeTop->getNode(), toInlineFully?" fully":""))
         {
         TR_InlineCall newInlineCall(optimizer(), this);
         newInlineCall.setSizeThreshold(size+100);
         bool inlineOK = newInlineCall.inlineCall(treeTop, 0, toInlineFully);

         if (inlineOK)
            {
            getOptData()->_totalInlinedBytecodeSize += size;
            inlinedSomething = true;
            if (trace())
               traceMsg(comp(), "inlined succeeded\n");
            }
         }
      }
   return inlinedSomething;
   }

static bool alreadyGoingToInline(TR::Node *callNode, TR_ScratchList<TR::TreeTop> &listOfCallTrees)
   {
   // A given callNode can appear under multiple treetops, so we can't just
   // compare treetop identities if we want to make sure we don't try to inline
   // the same function twice.
   //
   ListIterator<TR::TreeTop> iter(&listOfCallTrees);
   for (TR::TreeTop *callTree = iter.getFirst(); callTree; callTree = iter.getNext())
      {
      if (callTree->getNode()->getFirstChild() == callNode)
         return true;
      }
   return false;
   }

void TR_EscapeAnalysis::scanForExtraCallsToInline()
   {
   if (!_repeatAnalysis)
      {
      // This is the last pass of EA.  If there are any calls that we had
      // previously declined to inline to benefit EA, we longer need to
      // restrain ourselves, and can inline them now.
      //
      for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         if (  tt->getNode()->getOpCodeValue() == TR::BBStart
            && tt->getNode()->getBlock()->isCold())
            {
            // Don't bother inlining calls in cold blocks
            //
            tt = tt->getNode()->getBlock()->getExit();
            continue;
            }

         TR::TreeTop *callTreeToInline = NULL;
         TR::Node    *callNode = NULL;
         const char *reason = "??";
         if (  tt->getNode()->getNumChildren() >= 1
            && tt->getNode()->getFirstChild()->getOpCode().isCall()
            && tt->getNode()->getFirstChild()->getSymbol()->isResolvedMethod())
            {
            callNode = tt->getNode()->getFirstChild();
            if (!callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               switch (callNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod())
                  {
                  case TR::java_lang_Integer_valueOf:
                     callTreeToInline = tt;
                     reason = "dememoization did not eliminate it";
                     break;
                  default:
                     break;
                  }
               }
            }
         if (callTreeToInline && !alreadyGoingToInline(callNode, _inlineCallSites))
            {
            _inlineCallSites.add(callTreeToInline);
            if (trace())
               traceMsg(comp(), "Consider inlining %s n%dn [%p] of %s because %s\n", callNode->getOpCode().getName(), callNode->getGlobalIndex(), callNode, callNode->getSymbolReference()->getName(comp()->getDebug()), reason);
            }
         }
      }
   }

bool TR_EscapeAnalysis::alwaysWorthInlining(TR::Node *callNode)
   {
   // If this gets any more sophisticated, it should probably start sharing
   // code with alwaysWorthInlining from the inliner.
   //
   TR::ResolvedMethodSymbol *callee = callNode->getSymbol()->getResolvedMethodSymbol();
   if (callee) switch (callee->getRecognizedMethod())
      {
      case TR::java_lang_Integer_valueOf:
         return true;
      default:
         break;
      }
   return false;
   }

//TR::TreeTop * TR_EscapeAnalysis::findCallSiteFixed(TR::TreeTop * virtualCallSite)
bool TR_EscapeAnalysis::findCallSiteFixed(TR::TreeTop * virtualCallSite)
   {
   for (TR_CallSitesFixedMapper *cur = _fixedVirtualCallSites.getFirst(); cur; cur = cur->getNext())
      {
      if (cur->_vCallSite == virtualCallSite)
         {
         //return cur->_dCallSite;
         return true;
         }
      }

   //return NULL;
   return false;
   }


FieldInfo& Candidate::findOrSetFieldInfo(TR::Node *fieldRefNode, TR::SymbolReference *symRef, int32_t fieldOffset, int32_t fieldSize, TR::DataType fieldStoreType, TR_EscapeAnalysis *ea)
   {
   int32_t resultIdx = -1;

   TR::DataType refType = symRef->getSymbol()->getDataType();
   TR::DataType fieldType = refType;
   int N = 1;
   if (refType.isVector())
      {
      fieldType = refType.getVectorElementType();
      N = TR::Symbol::convertTypeToSize(refType)/TR::Symbol::convertTypeToSize(fieldType) ;
      }
   for (int j = 0; j < N; j++)
      {
      const bool isPeeking = (ea->_parms != NULL);

      int32_t i;
      if (!this->_fields)
         {
         this->_fields = new (trStackMemory()) TR_Array<FieldInfo>(trMemory(), 8, false, stackAlloc);
         i = -1;
         }
      else
         {
         for (i = this->_fields->size()-1; i >= 0; i--)
            {
            if (this->_fields->element(i)._offset == fieldOffset)
               {
               checkForDifferentSymRefs(this, i, symRef, ea, isPeeking);
               break;
               }
            }
         }
      if (i < 0)
         {
         i = this->_fields->size();
         (*this->_fields)[i]._offset = fieldOffset;
         if (comp()->useCompressedPointers() && fieldStoreType == TR::Address)
            fieldSize = TR::Compiler->om.sizeofReferenceField();
         (*this->_fields)[i]._symRef = NULL;
         (*this->_fields)[i]._size = fieldSize;
         (*this->_fields)[i]._vectorElem = 0;
         (*this->_fields)[i]._goodFieldSymrefs = new (trStackMemory()) TR_ScratchList<TR::SymbolReference>(trMemory());
         (*this->_fields)[i]._badFieldSymrefs  = new (trStackMemory()) TR_ScratchList<TR::SymbolReference>(trMemory());
         }
      if (resultIdx < 0)
         {
         resultIdx = i;
         }
      if (!isPeeking)
         {
         if (fieldRefNode != NULL)
            {
            (*this->_fields)[i].rememberFieldSymRef(fieldRefNode, this, ea);
            }
         else
            {
            (*this->_fields)[i].rememberFieldSymRef(symRef, ea);
            }
         }

      if (N > 1) // vector
         {
         (*this->_fields)[i]._vectorElem = j+1;
         fieldOffset += TR::Symbol::convertTypeToSize(fieldType);
         }
      }

   return this->_fields->element(resultIdx);
   }


void TR_EscapeAnalysis::printCandidates(const char *title)
   {
   if (title)
      traceMsg(comp(), "\n%s\n", title);

   int32_t index = 0;
   for (Candidate *candidate = _candidates.getFirst(); candidate; candidate = candidate->getNext())
      {
      traceMsg(comp(), "Candidate %d:\n", index++);
      candidate->print();
      }
   }

static void printSymRefList(TR_ScratchList<TR::SymbolReference> *list, TR::Compilation *comp)
   {
   ListIterator<TR::SymbolReference> iter(list);
   const char *sep = "";
   for (TR::SymbolReference *symRef = iter.getFirst(); symRef; symRef = iter.getNext())
      {
      traceMsg(comp, "%s#%d", sep, symRef->getReferenceNumber());
      sep = ",";
      }
   }

void Candidate::print()
   {
   traceMsg(comp(), "   Node = %p, contiguous = %d, local = %d\n", _node, isContiguousAllocation(), isLocalAllocation());
   if (_flags.getValue() != 0)
      {

#define PRINT_FLAG_IF(comp, cond, text) do { if (cond) traceMsg(comp, text " "); } while (false)
#define PRINT_FLAG(comp, query, text) PRINT_FLAG_IF(comp, query(), text)

      traceMsg(comp(), "   Flags = {");
      PRINT_FLAG(comp(), isLocalAllocation, "LocalAllocation");
      PRINT_FLAG(comp(), mustBeContiguousAllocation, "MustBeContiguous");
      PRINT_FLAG(comp(), isExplicitlyInitialized, "ExplicitlyInitialized");
      PRINT_FLAG(comp(), objectIsReferenced, "ObjectIsReferenced");
      PRINT_FLAG(comp(), fillsInStackTrace, "FillsInStackTrace");
      PRINT_FLAG(comp(), usesStackTrace, "UsesStackTrace");
      PRINT_FLAG(comp(), isInsideALoop, "InsideALoop");
      PRINT_FLAG(comp(), isInAColdBlock, "InAColdBlock");
      PRINT_FLAG(comp(), isProfileOnly, "ProfileOnly");
      PRINT_FLAG(comp(), callsStringCopyConstructor, "CallsStringCopy");
      PRINT_FLAG(comp(), forceLocalAllocation, "ForceLocalAllocation");
      traceMsg(comp(), "}\n");

#undef PRINT_FLAG_IF
#undef PRINT_FLAG
      }
   traceMsg(comp(), "   Value numbers = {");
   for (uint32_t j = 0; j <_valueNumbers->size(); j++)
      traceMsg(comp(), " %d", _valueNumbers->element(j));
   traceMsg(comp(), " }\n");
   if (isLocalAllocation() && hasCallSites())
      {
      traceMsg(comp(), "   Max inline depth = %d, inline bytecode size = %d\n", _maxInlineDepth, _inlineBytecodeSize);
      traceMsg(comp(), "   Call sites to be inlined:\n");
      ListIterator<TR::TreeTop> callSites(getCallSites());
      for (TR::TreeTop *callSite = callSites.getFirst(); callSite; callSite = callSites.getNext())
         {
         TR::Node *node = callSite->getNode()->getFirstChild();
         traceMsg(comp(), "      [%p] %s\n", node, node->getSymbol()->getMethodSymbol()->getMethod()->signature(trMemory()));
         }
      }
   if (_fields)
      {
      traceMsg(comp(), "   %d fields:\n", _fields->size());
      for (int32_t i = 0; i < _fields->size(); i++)
         {
         FieldInfo &field = _fields->element(i);
         traceMsg(comp(), "     %2d: offset=%-3d size=%-2d vectorElem=%-2d ",
            i, field._offset, field._size, field._vectorElem);
         if (field._symRef)
            traceMsg(comp(), "symRef=#%-4d ", field._symRef->getReferenceNumber());
         else
            traceMsg(comp(), "symRef=null ");
         traceMsg(comp(), "good={");
         printSymRefList(field._goodFieldSymrefs, comp());
         traceMsg(comp(), "} bad={");
         printSymRefList(field._badFieldSymrefs, comp());
         traceMsg(comp(), "}\n");
         }
      }
   }



#if CHECK_MONITORS
/* monitors */

// The following code is temporary fix for a problem with illegal monitor state exceptions.
// Escape Analysis must be careful about removing monent/monexit trees if there is a possibility
// of illegal monitor state.  This pass (conservatively) checks if the monitor structure may be
// illegal, returns true if it so.
//
// The real fix for this problem is to create the header-only object and let RedundantMonitorElimination
// deal with it, but that fix will be added in the next release (post R20).
//


bool TR_MonitorStructureChecker::checkMonitorStructure(TR::CFG *cfg)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _foundIllegalStructure = false;
   int32_t numBlocks = cfg->getNextNodeNumber();
   _seenNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _blockInfo = (int32_t *) trMemory()->allocateStackMemory(numBlocks * sizeof(int32_t), TR_Memory::EscapeAnalysis);
   memset(_blockInfo, -1, numBlocks * sizeof(int32_t));

   TR::CFGNode *start = cfg->getStart();
   TR::CFGNode *end   = cfg->getEnd();
   _blockInfo[start->getNumber()] = 0;
   _blockInfo[end  ->getNumber()] = 0;

   // Walk reverse post-order
   //
   TR_ScratchList<TR::CFGNode> stack;
   stack.add(start);
   while (!stack.isEmpty() && !_foundIllegalStructure)
      {
      TR::CFGNode *node = stack.popHead();

      if (_seenNodes->isSet(node->getNumber()))
         continue;

      if (node != start)
         {
         bool notDone = false;
         TR_PredecessorIterator pit(node);
         for (TR::CFGEdge *edge = pit.getFirst(); edge && !notDone; edge = pit.getNext())
            {
            TR::CFGNode *pred = edge->getFrom();
            if (!_seenNodes->isSet(pred->getNumber()))
                notDone = true;
            }

         if (notDone)
            continue;
         }

      _seenNodes->set(node->getNumber());
      processBlock(toBlock(node));

      if (node != end)
         {
         TR_SuccessorIterator sit(node);
         for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
            {
            TR::CFGNode *succ = edge->getTo();
            stack.add(succ);
            }
         }
      }

   return _foundIllegalStructure;
   }

void TR_MonitorStructureChecker::processBlock(TR::Block *block)
   {
   TR::TreeTop *exitTT = block->getExit();

   int32_t myInfo = _blockInfo[block->getNumber()];
   TR_ASSERT(myInfo != -1, "cfg walk failure"); // the cfg is a connected graph

   for (TR::TreeTop *tt = block->getEntry();
        tt != exitTT && !_foundIllegalStructure;
        tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();

      uint32_t exceptions = node->exceptionsRaised();
      if (exceptions)
         {
         for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
            {
            TR::Block *succ = toBlock((*edge)->getTo());
            if (succ->canCatchExceptions(exceptions))
               propagateInfoTo(succ, myInfo);
            }
         }

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCodeValue() == TR::NULLCHK)
         node = node->getFirstChild();

      if (node->getOpCodeValue() == TR::monent)
         myInfo++;

      if (node->getOpCodeValue() == TR::monexit)
         myInfo--;
      }

   TR_SuccessorIterator sit(block);
   for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      TR::CFGNode *succ = edge->getTo();
      propagateInfoTo(toBlock(succ), myInfo);
      }
   }

void TR_MonitorStructureChecker::propagateInfoTo(TR::Block *block, int32_t inInfo)
   {
   if (inInfo < 0)
      _foundIllegalStructure = true;
   else
      {
      int32_t thisInfo = _blockInfo[block->getNumber()];
      if (thisInfo == -1)
         _blockInfo[block->getNumber()] = inInfo;
      else
         if (inInfo != thisInfo)
            _foundIllegalStructure = true;
      }
   }
#endif

static TR_DependentAllocations *getDependentAllocationsFor(Candidate *c, List<TR_DependentAllocations> *dependentAllocations)
   {
   ListIterator<TR_DependentAllocations> dependentIt(dependentAllocations);
   TR_DependentAllocations *info;
   for (info = dependentIt.getFirst(); info; info=dependentIt.getNext())
      {
      if (info->getAllocation() == c)
         return info;
      }
   return NULL;
   }



static Candidate *getCandidate(TR_LinkHead<Candidate> *candidates, FlushCandidate *flushCandidate)
   {
   Candidate *candidate = flushCandidate->getCandidate();
   if (candidate || flushCandidate->getIsKnownToLackCandidate())
      {
      return candidate;
      }

   for (candidate = candidates->getFirst(); candidate; candidate = candidate->getNext())
      {
      if (flushCandidate->getAllocation() == candidate->_node)
         {
         flushCandidate->setCandidate(candidate);
         break;
         }
      }

   if (!candidate)
      {
      flushCandidate->setIsKnownToLackCandidate(true);
      }

   return candidate;
   }


TR_DataFlowAnalysis::Kind TR_FlowSensitiveEscapeAnalysis::getKind()
   {
   return FlowSensitiveEscapeAnalysis;
   }

TR_FlowSensitiveEscapeAnalysis *TR_FlowSensitiveEscapeAnalysis::asFlowSensitiveEscapeAnalysis()
   {
   return this;
   }

bool TR_FlowSensitiveEscapeAnalysis::supportsGenAndKillSets()
   {
   return false;
   }

int32_t TR_FlowSensitiveEscapeAnalysis::getNumberOfBits()
   {
   return _numAllocations;
   }


bool TR_FlowSensitiveEscapeAnalysis::getCFGBackEdgesAndLoopEntryBlocks(TR_Structure *structure)
   {
   if (!structure->asBlock())
      {
      TR_RegionStructure *region = structure->asRegion();
      bool isLoop = region->isNaturalLoop();

      //if (!isLoop &&
      //    !region->isAcyclic())
      //   return true;

      if (isLoop)
         {
         collectCFGBackEdges(region->getEntry());
         _loopEntryBlocks->set(region->getEntry()->getNumber());
         if (trace())
            traceMsg(comp(), "Block numbered %d is loop entry\n", region->getEntry()->getNumber());
         }

      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*region);
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         subStruct = subNode->getStructure();
         if (getCFGBackEdgesAndLoopEntryBlocks(subStruct))
            return true;
         }
      }
   else
      {
      if (structure->asBlock()->getBlock()->isCatchBlock())
         _catchBlocks->set(structure->getNumber());
      }

   return false;
   }



void TR_FlowSensitiveEscapeAnalysis::collectCFGBackEdges(TR_StructureSubGraphNode *loopEntry)
   {
   for (auto edge = loopEntry->getPredecessors().begin(); edge != loopEntry->getPredecessors().end(); ++edge)
      {
      TR_Structure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure();
      pred->collectCFGEdgesTo(loopEntry->getNumber(), &_cfgBackEdges);
      }
   }


TR_FlowSensitiveEscapeAnalysis::TR_FlowSensitiveEscapeAnalysis(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, TR_EscapeAnalysis *escapeAnalysis)
   : TR_IntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, escapeAnalysis->trace()),
     _cfgBackEdges(comp->trMemory()),
     _flushEdges(comp->trMemory()),
     _splitBlocks(comp->trMemory())
   {
   if (trace())
      traceMsg(comp, "Starting FlowSensitiveEscapeAnalysis analysis\n");

   if (comp->getVisitCount() > 8000)
      comp->resetVisitCounts(1);

   // Find the number of allocations and assign an index to each
   //
   _escapeAnalysis = escapeAnalysis;
   _candidates = &(escapeAnalysis->_candidates);
   _numAllocations = 0;
   _newlyAllocatedObjectWasLocked = false;
   _cfgBackEdges.deleteAll();
   _flushEdges.deleteAll();
   _splitBlocks.deleteAll();

   _flushCandidates = new (trStackMemory()) TR_LinkHead<FlushCandidate>;
   _flushCandidates->setFirst(NULL);

   Candidate *candidate, *next;
   for (candidate = _candidates->getFirst(); candidate; candidate = next)
      {
      next = candidate->getNext();
      if (trace())
         traceMsg(comp, "Allocation node %p is represented by bit position %d\n", candidate->_node, _numAllocations);
      candidate->_index = _numAllocations++;
      }

   if (trace())
      traceMsg(comp, "_numAllocations = %d\n", _numAllocations);

   if (_numAllocations == 0)
      return; // Nothing to do if there are no allocations

   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();

   // After this point all stack allocation will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (!performAnalysis(rootStructure, false))
      {
      return;
      }

   int32_t i;
   if (trace())
      {
      for (i = 1; i < _numberOfNodes; ++i)
         {
         if (_blockAnalysisInfo[i])
            {
            traceMsg(comp, "\nSolution for block_%d: ",i);
            _blockAnalysisInfo[i]->print(comp);
            }
         }
      traceMsg(comp, "\nEnding FlowSensitiveEscapeAnalysis analysis\n");
      }

   int32_t blockNum = -1;
   TR::TreeTop *treeTop;
   TR_BitVector *blockInfo = NULL;
   for (treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         blockNum = node->getBlock()->getNumber();
         blockInfo = _blockAnalysisInfo[blockNum];

         //if (_blocksWithFlushes->get(blockNum) &&
         //    blockInfo &&
         //    !blockInfo->isEmpty())
         //   {
         //   printf("%d allocation(s) reached allocation in block_%d in method %s\n", blockInfo->elementCount(), blockNum, comp->signature();
         //   blockInfo->print(comp);
         //   fflush(stdout);
         //   }

         continue;
         }

      if (node->getOpCode().isNullCheck() ||
          node->getOpCode().isResolveCheck() ||
          (node->getOpCodeValue() == TR::treetop))
         node = node->getFirstChild();

      if ((node->getOpCodeValue() == TR::monent) || (node->getOpCodeValue() == TR::monexit))
         {
         Candidate *candidate;
         for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
            {
            if (escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node) == escapeAnalysis->_valueNumberInfo->getValueNumber(node->getFirstChild()))
               {
               if (blockInfo &&
                   blockInfo->get(candidate->_index) &&
                   performTransformation(comp, "%sMark monitor node [%p] as a local object monitor (flow sensitive escape analysis)\n",OPT_DETAILS, node))
                  {
                  //if (!node->isSyncMethodMonitor())
                     {
                     node->setLocalObjectMonitor(true);
                     optimizer->setRequestOptimization(OMR::redundantMonitorElimination);
                     //printf("Eliminating monitor in %s\n", comp->signature());
                     }
                  //else
                  //   {
                  //printf("Removing monitor from %s\n", comp->signature());
                  //   if (treeTop->getNode() == node)
                  //      TR::Node::recreate(node, TR::treetop);
                  //   else
                  //      TR::Node::recreate(node, TR::PassThrough);
                  //   }
                  }
               break;
               }
            }
         }
      }

   if (_flushCandidates->isEmpty())
      {
      return;
      }

   if (trace())
      traceMsg(comp, "\nStarting local flush elimination \n");

   TR_LocalFlushElimination localFlushElimination(_escapeAnalysis, _numAllocations);
   localFlushElimination.perform();

   if (trace())
      traceMsg(comp, "\nStarting global flush elimination \n");

   if (comp->getFlowGraph()->getStructure()->markStructuresWithImproperRegions())
      {
      return;
      }

   int32_t numBlocks = comp->getFlowGraph()->getNextNodeNumber();
   _successorInfo = (TR_BitVector **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR_BitVector *));
   memset(_successorInfo, 0, numBlocks * sizeof(TR_BitVector *));
   _predecessorInfo = (TR_BitVector **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR_BitVector *));
   memset(_predecessorInfo, 0, numBlocks * sizeof(TR_BitVector *));
   for (TR::CFGNode *node = comp->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      _successorInfo[node->getNumber()] = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
      _predecessorInfo[node->getNumber()] = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
      }

   TR_BitVector *visitedNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _loopEntryBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _catchBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   getCFGBackEdgesAndLoopEntryBlocks(comp->getFlowGraph()->getStructure());
   TR::MonitorElimination::collectPredsAndSuccs(comp->getFlowGraph()->getStart(), visitedNodes, _predecessorInfo, _successorInfo, &_cfgBackEdges, _loopEntryBlocks, comp);

   _scratch2 = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   _scratch = visitedNodes;
   _scratch->empty();
   //_scratch2 = _loopEntryBlocks;
   *_scratch2 = *_loopEntryBlocks;
   *_scratch2 |= *_catchBlocks;
   //_scratch2->empty();

   TR_BitVector *movedToBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   FlushCandidate *flushCandidate;
   for (flushCandidate = _flushCandidates->getFirst(); flushCandidate; flushCandidate = flushCandidate->getNext())
      {
      // If this flushCanidate has been associated with more then one allocation
      // then we should no longer consider it as a canidate for being disabled
      if (flushCandidate->getFlush()->getNode()->getAllocation()==NULL)
         continue;
      candidate = getCandidate(_candidates, flushCandidate);

      if (!candidate)
         continue;


      //TR::Block *block = candidate->_block;
      //int32_t blockNum = block->getNumber();
      int32_t blockNum = flushCandidate->getBlockNum();

      if (trace())
         traceMsg(comp, "\nConsidering Flush for allocation %p (index %d) in block_%d\n", candidate->_node, candidate->_index, blockNum);

      TR_BitVector *successors = _successorInfo[blockNum];

       if (trace())
         {
         traceMsg(comp, "Successors : \n");
         successors->print(comp);
         traceMsg(comp, "\n");
         }

      TR_BitVectorIterator succIt(*successors);
      while (succIt.hasMoreElements())
         {
         int32_t nextSucc = succIt.getNextElement();
         TR_BitVector *succInfo = _blockAnalysisInfo[nextSucc];

         *_scratch2 = *_loopEntryBlocks;
         *_scratch2 |= *_catchBlocks;

          if (trace())
             traceMsg(comp, "Successor %d being examined\n", nextSucc);

         if ((_blocksWithFlushes->get(nextSucc) ||
              _blocksWithSyncs->get(nextSucc)) &&
              succInfo &&
              succInfo->get(candidate->_index))
            {
            if (trace())
               traceMsg(comp, "Current allocation %d reaches successor %d\n", candidate->_index, nextSucc);

            TR_BitVector *preds = _predecessorInfo[nextSucc];

            if (trace())
               {
               traceMsg(comp, "Predecessors of next succ %d : \n", nextSucc);
               preds->print(comp);
               traceMsg(comp, "\n");
               }

            *_scratch = *preds;
            *_scratch &= *successors;

            if (_scratch2->get(nextSucc))
               continue;

            *_scratch2 &= *_scratch;
            if (!_scratch2->isEmpty())
               continue;

            _scratch->set(blockNum);

            bool postDominated = true;
            TR::CFG *cfg = comp->getFlowGraph();
            TR::CFGNode *nextNode;
            for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
              {
              if (_scratch->get(nextNode->getNumber()))
                 {
                 for (auto succ = nextNode->getSuccessors().begin(); succ != nextNode->getSuccessors().end(); ++succ)
                    {
                    if (trace())
                       traceMsg(comp, "Checking succ edge from %d to %d\n", nextNode->getNumber(), (*succ)->getTo()->getNumber());

                    if (!_scratch->get((*succ)->getTo()->getNumber()) &&
                        ((*succ)->getTo()->getNumber() != nextSucc))
                       {
                       postDominated = false;
                       _flushEdges.add(new (trStackMemory()) TR_CFGEdgeAllocationPair(*succ, candidate));
                       if (trace())
                          traceMsg(comp, "Adding flush edge from %d to %d\n", nextNode->getNumber(), (*succ)->getTo()->getNumber());
                       }
                    }
                 }
              }

            if (trace())
               {
               traceMsg(comp, "Scratch : \n");
               _scratch->print(comp);
               traceMsg(comp, "\n");
               }

            //if (postDominated)
               {
               if (trace())
                  traceMsg(comp, "Current allocation %d is post dominated by allocation in successor %d\n", candidate->_index, nextSucc);

               Candidate *succCandidate = NULL;
               FlushCandidate *succFlushCandidate = NULL;
               if (!_blocksWithSyncs->get(nextSucc))
                  {
                  for (succFlushCandidate = _flushCandidates->getFirst(); succFlushCandidate; succFlushCandidate = succFlushCandidate->getNext())
                    //for (succCandidate = _candidates->getFirst(); succCandidate; succCandidate = succCandidate->getNext())
                     {
                     succCandidate = getCandidate(_candidates, succFlushCandidate);
                     if (!succCandidate)
                        continue;

                     //TR::Block *succBlock = succCandidate->_block;
                     int32_t succBlockNum = succFlushCandidate->getBlockNum();

                     //if (trace())
                     //   traceMsg(comp, "succCandidate %p succCandidate num %d succCandidate Flush reqd %d succBlockNum %d\n", succCandidate, succCandidate->_index, succCandidate->_flushRequired, succBlockNum);

                     if ((succBlockNum == nextSucc) &&
                         succCandidate->_flushRequired)
                        {
                        bool nextAllocCanReach = true;

                        ListIterator<Candidate> candIt(&candidate->_flushMovedFrom);
                        for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                           {
                           if (!succInfo->get(dependentCandidate->_index))
                              {
                              nextAllocCanReach = false;
                              break;
                              }
                           }

                        //if (trace())
                        //    traceMsg(comp, "succ candidate %p nextAllocCanReach %d\n", succCandidate, nextAllocCanReach);

                        if (nextAllocCanReach)
                           {
                           if (succCandidate != candidate)
                              {
                              succCandidate->_flushMovedFrom.add(candidate);
                              ListIterator<Candidate> candIt(&candidate->_flushMovedFrom);
                              for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                                 succCandidate->_flushMovedFrom.add(candidate);
                              }
                           break;
                           }
                        }
                     }
                  }

               //if (trace())
               //   traceMsg(comp, "succCandidate %p _blocksWithSyncs bit %d\n", succCandidate, _blocksWithSyncs->get(nextSucc));
               TR::TreeTop *tt = _syncNodeTTForBlock[nextSucc];
               // If we have a sync node where the flush is not already optimally placed or we have a successor flush candidate
               if ((_blocksWithSyncs->get(nextSucc) && flushCandidate->getFlush()->getNode() != tt->getPrevTreeTop()->getNode()) ||
                   succCandidate)
                  {
                  movedToBlocks->set(nextSucc);
                  candidate->_flushRequired = false;
                  TR::TreeTop *flushTree = flushCandidate->getFlush();
                  TR::TreeTop *prevTree = flushTree->getPrevTreeTop();
                  TR::TreeTop *nextTree = flushTree->getNextTreeTop();
                  if ((prevTree->getNextTreeTop() == flushTree) &&
                                  (nextTree->getPrevTreeTop() == flushTree))
                     {
                     if (flushTree->getNode()->getOpCodeValue() == TR::allocationFence)
                        {
                        TR_ASSERT_FATAL(flushTree->getNode()->getAllocation()!=NULL || flushTree->getNode()->canOmitSync()==true, "EA: Attempt to omit sync on a fence (%p) with allocation already set to ALL.\n", flushTree->getNode());
                        flushTree->getNode()->setOmitSync(true);
                        if (trace())
                           traceMsg(comp,"Setting AF node %p for allocation %p to omitsync\n", flushTree->getNode(), flushTree->getNode()->getAllocation() );
                        flushTree->getNode()->setAllocation(NULL);
                        }
                     //prevTree->join(nextTree);
                     }

                  if (!succCandidate)
                     {
                     TR::Node *afNode = tt->getPrevTreeTop()->getNode();
                     if (afNode->getOpCodeValue() == TR::allocationFence)
                        {
                        if (afNode->canOmitSync()==true)
                           {
                           afNode->setAllocation(candidate->_node);
                           afNode->setOmitSync(false);
                           if (trace())
                              traceMsg(comp, "Restoring AF node %p for allocation %p above node %p.\n", afNode, candidate->_node, tt->getNode());
                           }
                        else
                           {
                           afNode->setAllocation(NULL); // Existing AllocationFence is now needed by more then one allocation
                           if (trace())
                              traceMsg(comp, "Setting AF node %p allocation to ALL above %p.\n", afNode, tt->getNode() );
                           }
                        }
                     else
                        {
                        // Insert an AllocationFence above the monExit/volatile-access. CodeGen can optimize the fence & monExit/volatile-access sequence
                        // so that only one flush instruction is executed. With lock-reservation or lock-reentry it's possible for a monExit to skip the
                        // flush so this AllocationFence will ensure correct behaviour. In addition, if the monExit/volatile-access happens to be
                        // moved/removed later in the compile, this AllocationFence will ensure a flush instruction is generated.
                        TR::Node *afNode = TR::Node::createAllocationFence(candidate->_node, candidate->_node);
                        tt->insertBefore(TR::TreeTop::create(comp, afNode, NULL, NULL));
                        if (trace())
                           traceMsg(comp,   "Inserted AF node %p for candidate %p above %p (%s).\n", afNode, candidate->_node, tt->getNode(), tt->getNode()->getOpCode().getName() );
                        //fprintf( stderr, "Inserted AF node %p above %p (%s).\n", afNode, tt->getNode(), tt->getNode()->getOpCode().getName() );
                        }
                     }
                  //if (trace())
                  //   {
                  //   traceMsg(comp, "0reaching candidate %p index %d does not need Flush\n", candidate, candidate->_index);
                  //   }

                  if (trace())
                     {
                     if (succCandidate)
                        {
                        traceMsg(comp, "Flush for current allocation %d is post dominated by (and moved to) Flush for allocation %d\n", candidate->_index, succCandidate->_index);
                        //printf("Moved flush for allocation %p in block_%d to allocation %p in succ block_%d in method %s\n", candidate->_node, blockNum, succCandidate->_node, nextSucc, comp->signature());
                        }
                     else
                        {
                        traceMsg(comp, "Flush for current allocation %d is post dominated by (and moved to) real sync for in succ %d\n", candidate->_index, nextSucc);
                        //printf("Moved flush for allocation %p in block_%d to real sync in succ %d in method %s\n", candidate->_node, blockNum, nextSucc, comp->signature());
                        }
                     fflush(stdout);
                     }
                  //break;
                  }
               }
            }
         }

      ListElement<TR_CFGEdgeAllocationPair> *listElem;
      ListElement<TR_CFGEdgeAllocationPair> *nextListElem = NULL, *prevListElem = NULL;
      TR_CFGEdgeAllocationPair *pair = NULL;
      for (listElem = _flushEdges.getListHead(); listElem != NULL;)
         {
         nextListElem = listElem->getNextElement();
         TR_CFGEdgeAllocationPair *pair = listElem->getData();
         if (trace())
            {
            traceMsg(comp, "Processing flush edge from %d to %d\n", pair->getEdge()->getFrom()->getNumber(), pair->getEdge()->getTo()->getNumber());
            traceMsg(comp, "Processing flush alloc %p vs candidate %p\n", pair->getAllocation(), candidate);
            }

         if (pair->getAllocation() == candidate)
            {
            bool edgeSplitRequired = true;
            int32_t toNum = pair->getEdge()->getTo()->getNumber();
            if (movedToBlocks->get(toNum))
               edgeSplitRequired = false;

            if (!edgeSplitRequired)
               {
               if (prevListElem)
                  prevListElem->setNextElement(nextListElem);
               else
                  _flushEdges.setListHead(nextListElem);
               listElem = nextListElem;
               continue;
               }
            }

         prevListElem = listElem;
         listElem = nextListElem;
         }
      }


   ListIterator<TR_CFGEdgeAllocationPair> pairIt(&_flushEdges);
   for (TR_CFGEdgeAllocationPair *pair = pairIt.getFirst(); pair; pair = pairIt.getNext())
      {
      TR::CFGNode *to = pair->getEdge()->getTo();
      TR::Block *splitBlock = NULL;
      if (to == comp->getFlowGraph()->getEnd())
         {
         splitBlock = toBlock(pair->getEdge()->getFrom());
         }
      else if ((to->getPredecessors().size() == 1))
         {
         splitBlock = toBlock(to);
         if (trace())
            {
            traceMsg(comp, "For edge %d->%d adding Flush in block_%d for candidate %p index %d\n", pair->getEdge()->getFrom()->getNumber(), pair->getEdge()->getTo()->getNumber(), splitBlock->getNumber(), pair->getAllocation(), pair->getAllocation()->_index);
            //printf("For edge %d->%d adding Flush in block_%d for candidate %p index %d\n", pair->getEdge()->getFrom()->getNumber(), pair->getEdge()->getTo()->getNumber(), splitBlock->getNumber(), pair->getAllocation(), pair->getAllocation()->_index);
            }
         }
      else
         {
         splitBlock = findOrSplitEdge(toBlock(pair->getEdge()->getFrom()), toBlock(to));
         if (trace())
            {
            traceMsg(comp, "Splitting edge %d->%d with block_%d for candidate %p index %d\n", pair->getEdge()->getFrom()->getNumber(), pair->getEdge()->getTo()->getNumber(), splitBlock->getNumber(), pair->getAllocation(), pair->getAllocation()->_index);
            //printf("Splitting edge %d->%d with block_%d for candidate %p index %d\n", pair->getEdge()->getFrom()->getNumber(), pair->getEdge()->getTo()->getNumber(), splitBlock->getNumber(), pair->getAllocation(), pair->getAllocation()->_index);
            }
         }

      TR::Node *firstNode = splitBlock->getFirstRealTreeTop()->getNode();
      if (firstNode->getOpCodeValue() == TR::allocationFence)
         firstNode->setAllocation(NULL);
      else
         splitBlock->prepend(TR::TreeTop::create(comp, TR::Node::createAllocationFence(pair->getAllocation()->_node, pair->getAllocation()->_node), NULL, NULL));
      }

   for (flushCandidate = _flushCandidates->getFirst(); flushCandidate; flushCandidate = flushCandidate->getNext())
      {
      candidate = getCandidate(_candidates, flushCandidate);

      if (!candidate)
         continue;

      if (!candidate->_flushMovedFrom.isEmpty())
         {
         flushCandidate->getFlush()->getNode()->setAllocation(NULL);
         }
      }
   }

bool TR_FlowSensitiveEscapeAnalysis::postInitializationProcessing()
   {
   _blocksWithSyncs = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   _syncNodeTTForBlock   =  new (trStackMemory()) TR::TreeTop*[_numberOfNodes];
   memset(_syncNodeTTForBlock,   0, _numberOfNodes * sizeof(TR::TreeTop*));
   int32_t blockNum = -1;
   TR::TreeTop *treeTop = NULL;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         blockNum = node->getBlock()->getNumber();
         continue;
         }

      if ((node->getOpCodeValue() == TR::allocationFence) &&
          node->getAllocation())
         {
         FlushCandidate *candidate = new (trStackMemory()) FlushCandidate(treeTop, node->getAllocation(), blockNum);
         _flushCandidates->add(candidate);
         }

      if (node->getOpCode().isNullCheck() ||
          node->getOpCode().isResolveCheck() ||
          (node->getOpCodeValue() == TR::treetop))
         node = node->getFirstChild();

      if (node->getOpCodeValue() == TR::monent)
         {
         Candidate *candidate;
         for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
            {
            if (_escapeAnalysis->_valueNumberInfo->getValueNumber(node->getFirstChild()) == _escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node))
               _newlyAllocatedObjectWasLocked = true;
            }
         }
      else if (node->getOpCodeValue() == TR::monexit)
         {
         bool syncPresent = true;
         Candidate *candidate;
         for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
            {
            if (_escapeAnalysis->_valueNumberInfo->getValueNumber(node->getFirstChild()) == _escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node))
               {
               syncPresent = false;
               break;
               }
            }

         if (syncPresent)
            {
            _blocksWithSyncs->set(blockNum);
            // Keep track of this sync node treetop if we don't already have one for this block
            if (!_syncNodeTTForBlock[blockNum])
               _syncNodeTTForBlock[blockNum] = treeTop;
            }
         }
      // Due to lock-reservation and lock reentry it's not guaranteed that a synchronized method will issue a memory flush
      //else if (node->getOpCode().isCall() && !node->hasUnresolvedSymbolReference() &&
      //         node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isSynchronised())
      //   {
      //   _blocksWithSyncs->set(blockNum);
      //   _syncNodeTTForBlock[blockNum] = treeTop;
      //   }
      }

   int32_t i;

   if (!_newlyAllocatedObjectWasLocked &&
       _flushCandidates->isEmpty())
      {
      return false;
      }

   _blocksWithFlushes = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   FlushCandidate *flushCandidate;
   for (flushCandidate = _flushCandidates->getFirst(); flushCandidate; flushCandidate = flushCandidate->getNext())
      _blocksWithFlushes->set(flushCandidate->getBlockNum());

   //_blocksThatNeedFlush = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc, growable);
   return true;
   }


TR::Block *TR_FlowSensitiveEscapeAnalysis::findOrSplitEdge(TR::Block *from, TR::Block *to)
   {
   TR::Block *splitBlock = NULL;
   if (!from->hasSuccessor(to))
      {
      for (auto edge2 = to->getPredecessors().begin(); edge2 != to->getPredecessors().end(); ++edge2)
         {
         if (_splitBlocks.find((*edge2)->getFrom()) &&
             from->hasSuccessor((*edge2)->getFrom()))
            {
            splitBlock = toBlock((*edge2)->getFrom());
            break;
            }
         }
      }
   else
      {
      splitBlock = from->splitEdge(from, to, comp());
      _splitBlocks.add(splitBlock);
      }

   return splitBlock;
   }




void TR_FlowSensitiveEscapeAnalysis::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR::Block *block = blockStructure->getBlock();
   if ((block == comp()->getFlowGraph()->getStart()) ||
       (block == comp()->getFlowGraph()->getEnd()))
      return;

   int32_t blockNum = block->getNumber();
   bool seenException = false;

   comp()->incVisitCount(); //@TODO: slightly untrivial as it requires either a change to API (i.e. analyzeNode)
                            //or an extra field needs to be declared in TR_FlowSensitiveEscapeAnalysis
                            //so let's leave this use as is for the next iteration
   TR::TreeTop *lastTree = block->getExit()->getNextTreeTop();
   for (TR::TreeTop *treeTop = block->getEntry(); treeTop != lastTree; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         continue;

#if DEBUG
      if (node->getOpCodeValue() == TR::BBEnd && trace())
         {
         traceMsg(comp(), "\n  Block %d:\n", blockNum);
         traceMsg(comp(), "     Normal set ");
         if (_regularInfo)
            _regularInfo->print(comp());
         else
            traceMsg(comp(), "{}");
         traceMsg(comp(), "\n     Exception set ");
         if (_exceptionInfo)
            _exceptionInfo->print(comp());
         else
            traceMsg(comp(), "{}");
         }
#endif

      analyzeNode(node, treeTop, seenException, blockNum, NULL);

      if (!seenException && treeHasChecks(treeTop))
         seenException = true;
      }
   copyFromInto(_regularInfo, _blockAnalysisInfo[blockStructure->getNumber()]);
   }


void TR_FlowSensitiveEscapeAnalysis::analyzeNode(TR::Node *node, TR::TreeTop *treeTop, bool seenException, int32_t blockNum, TR::Node *parent)
   {
   // Update gen and kill info for nodes in this subtree
   //
   int32_t i;

   if (node->getVisitCount() == comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   // Process the children first
   //
   for (i = node->getNumChildren()-1; i >= 0; --i)
      {
      analyzeNode(node->getChild(i), treeTop, seenException, blockNum, node);
      }


   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if (symReference->getSymbol()->isVolatile())
         {
         _blocksWithSyncs->set(blockNum);
         // Keep track of this volatile load/store node tt, prefer volatile load/store over monexits since monexits can skip sync in some cases
         _syncNodeTTForBlock[blockNum] = treeTop;
         }
      }

   //traceMsg(comp(), "Node %p is being examined\n", node);

   TR_EscapeAnalysis *escapeAnalysis = _escapeAnalysis;
   if (!node->getOpCode().isCall())
      {
      int32_t     valueNumber = 0;
      Candidate  *candidate   = NULL;
      TR::Node   *child       = NULL;
      TR_DependentAllocations *dependencies = NULL;

      if ((node->getOpCodeValue() == TR::areturn) ||
          (node->getOpCodeValue() == TR::athrow))
         child = node->getFirstChild();
      else if (node->getOpCode().isStoreIndirect())
         {
         child = node->getSecondChild();

         TR::Node *base = node->getFirstChild();
         int32_t baseValueNumber = escapeAnalysis->_valueNumberInfo->getValueNumber(base);
         Candidate *candidate;
         for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
            {
            if (escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node) == baseValueNumber)
               {
               if (_regularInfo->get(candidate->_index))
                  {
                    //child = NULL;
                  dependencies = getDependentAllocationsFor(candidate, &(_escapeAnalysis->_dependentAllocations));
                  if (!dependencies)
                     {
                     dependencies = new (trStackMemory()) TR_DependentAllocations(candidate, 0, trMemory());
                     _escapeAnalysis->_dependentAllocations.add(dependencies);
                     }
                  }
               break;
               }
            }
         }
      else if (node->getOpCode().isStore() &&
               node->getSymbolReference()->getSymbol()->isStatic())
         child = node->getFirstChild();

      if (child)
         {
         // If we are in a called method, returning a candidate to the caller
         // escapes, since we don't track what happens to the returned value.
         //
         valueNumber = escapeAnalysis->_valueNumberInfo->getValueNumber(child);
         }

      for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
         {
         if (child &&
             escapeAnalysis->usesValueNumber(candidate, valueNumber))
            {
            if (!dependencies)
               {
               _regularInfo->reset(candidate->_index);
                if (seenException)
                  _exceptionInfo->reset(candidate->_index);

               TR_DependentAllocations *deps = getDependentAllocationsFor(candidate, &(escapeAnalysis->_dependentAllocations));
               if (deps)
                  {
                  ListIterator<Candidate> candIt(deps->getDependentAllocations());
                  for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                     {
                     _regularInfo->reset(dependentCandidate->_index);
                     if (seenException)
                        _exceptionInfo->reset(dependentCandidate->_index);
                     }
                  }
               }
            else
               dependencies->addDependentAllocation(candidate);
            }

         if (node == candidate->_node)
            {
            _regularInfo->set(candidate->_index);
            if (!seenException)
               _exceptionInfo->set(candidate->_index);
            }
         }
      }
   else
      {
      int32_t firstArgIndex = node->getFirstArgumentIndex();
      for (int32_t arg = firstArgIndex; arg < node->getNumChildren(); arg++)
         {
         TR::Node *child = node->getChild(arg);
         int32_t valueNumber = escapeAnalysis->_valueNumberInfo->getValueNumber(child);
         Candidate *candidate;
         for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
            {
            if (escapeAnalysis->usesValueNumber(candidate, valueNumber))
               {
               _regularInfo->reset(candidate->_index);
               if (seenException)
                  _exceptionInfo->reset(candidate->_index);

               TR_DependentAllocations *deps = getDependentAllocationsFor(candidate, &(escapeAnalysis->_dependentAllocations));
               if (deps)
                  {
                  ListIterator<Candidate> candIt(deps->getDependentAllocations());
                  for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                     {
                     _regularInfo->reset(dependentCandidate->_index);
                     if (seenException)
                        _exceptionInfo->reset(dependentCandidate->_index);
                     }
                  }
               }
            }
         }
      }
   }



TR_LocalFlushElimination::TR_LocalFlushElimination(TR_EscapeAnalysis *escapeAnalysis, int32_t numAllocations)
   : _dependentAllocations(escapeAnalysis->trMemory())
   {
   _escapeAnalysis = escapeAnalysis;
   _numAllocations = numAllocations;
   }

int32_t TR_LocalFlushElimination::perform()
   {
   if (_escapeAnalysis)
      _candidates = &(_escapeAnalysis->_candidates);
   else
      {
      _candidates = new (trStackMemory()) TR_LinkHead<Candidate>;
      _numAllocations = -1;
      }

   _flushCandidates = new (trStackMemory()) TR_LinkHead<FlushCandidate>;
   _flushCandidates->setFirst(NULL);

   TR::NodeChecklist visited(comp());
   TR::TreeTop *treeTop;
   TR::Block *block = NULL;
   _dependentAllocations.deleteAll();

   if (_numAllocations < 0)
      {
      _numAllocations = 0;
      for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextRealTreeTop())
         {
         // Get information about this block
         //
         TR::Node *node = treeTop->getNode();
         if (node->getOpCodeValue() == TR::BBStart)
            block = node->getBlock();

         if ((node->getOpCodeValue() == TR::treetop) &&
             ((node->getFirstChild()->getOpCodeValue() == TR::New) ||
              (node->getFirstChild()->getOpCodeValue() == TR::newvalue) ||
              (node->getFirstChild()->getOpCodeValue() == TR::newarray) ||
              (node->getFirstChild()->getOpCodeValue() == TR::anewarray)))
            {
            Candidate *candidate = new (trStackMemory()) Candidate(node, treeTop, block, -1, NULL, comp());
            _candidates->add(candidate);
            candidate->_index = _numAllocations++;
            }
         }
      }

   _allocationInfo = new (trStackMemory()) TR_BitVector(_numAllocations, trMemory(), stackAlloc);
   _temp = new (trStackMemory()) TR_BitVector(_numAllocations, trMemory(), stackAlloc);

   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextRealTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         block = node->getBlock();

      if ((node->getOpCodeValue() == TR::allocationFence) &&
          node->getAllocation())
         {
         FlushCandidate *candidate = new (trStackMemory()) FlushCandidate(treeTop, node->getAllocation(), block->getNumber());
         _flushCandidates->add(candidate);
         }
      }

   // Process each block in treetop order
   //
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextRealTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         _allocationInfo->empty();
         }

      examineNode(node, treeTop, block, visited);
      }

   FlushCandidate *flushCandidate;
   for (flushCandidate = _flushCandidates->getFirst(); flushCandidate; flushCandidate = flushCandidate->getNext())
      {
      Candidate *candidate = getCandidate(_candidates, flushCandidate);

      if (!candidate)
         continue;

      if (!candidate->_flushMovedFrom.isEmpty())
         {
         flushCandidate->getFlush()->getNode()->setAllocation(NULL);
         }
      }

   return 1; // actual cost
   }


bool TR_LocalFlushElimination::examineNode(TR::Node *node, TR::TreeTop *tt, TR::Block *currBlock, TR::NodeChecklist& visited)
   {
   if (visited.contains(node))
      return true;
   visited.add(node);

   TR::ILOpCode &opCode = node->getOpCode();

   if (!opCode.isCall())
      {
      int32_t    valueNumber = -1;
      Candidate *candidate;
      TR::Node   *child = NULL;
      TR_DependentAllocations *dependencies = NULL;
      if ((node->getOpCodeValue() == TR::areturn) ||
          (node->getOpCodeValue() == TR::athrow))
         child = node->getFirstChild();
      else if (node->getOpCode().isStoreIndirect())
         {
         child = node->getSecondChild();

         TR::Node *base = node->getFirstChild();

         //if (_escapeAnalysis)
            {
            int32_t baseValueNumber = -1;
            if (_escapeAnalysis)
               baseValueNumber = _escapeAnalysis->_valueNumberInfo->getValueNumber(base);
            Candidate *candidate;
            for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
               {
               if ((_escapeAnalysis &&
                    (_escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node) == baseValueNumber)) ||
                   !_escapeAnalysis)
                  {
                  if (_allocationInfo->get(candidate->_index))
                    {
                    TR_ScratchList<TR_DependentAllocations> *list = NULL;
                    if (_escapeAnalysis)
                       list = &(_escapeAnalysis->_dependentAllocations);
                    else
                       list = &_dependentAllocations;
                    dependencies = getDependentAllocationsFor(candidate, list);
                    if (!dependencies)
                       {
                       dependencies = new (trStackMemory()) TR_DependentAllocations(candidate, 0, trMemory());
                       list->add(dependencies);
                       }
                    }
                  //child = NULL;
                  break;
                  }
               }
            }
         }
      else if (node->getOpCode().isStore() &&
               node->getSymbolReference()->getSymbol()->isStatic())
         child = node->getFirstChild();


      if (_escapeAnalysis)
         {
         if (child)
            {
            // If we are in a called method, returning a candidate to the caller
            // escapes, since we don't track what happens to the returned value.
            //
            valueNumber = _escapeAnalysis->_valueNumberInfo->getValueNumber(child);
            }
         }

      bool nodeHasSync = false;
      bool nodeHasVolatile = false;
      if (node->getOpCodeValue() == TR::monexit)
         {
         bool syncPresent = true;
         if (_escapeAnalysis)
            {
            Candidate *candidate;
            for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
               {
               if (_escapeAnalysis->_valueNumberInfo->getValueNumber(node->getFirstChild()) == _escapeAnalysis->_valueNumberInfo->getValueNumber(candidate->_node))
                    {
                    syncPresent = false;
                    break;
                    }
               }
            }

         if (syncPresent)
            nodeHasSync = true;
         }
      else
         {
         TR::ILOpCode &opCode = node->getOpCode();
         if (opCode.hasSymbolReference())
            {
            TR::SymbolReference *symReference = node->getSymbolReference();
            if (symReference->getSymbol()->isVolatile())
               {
               nodeHasSync = true;
               nodeHasVolatile = true;
               }
            }
         // Due to lock-reservation and lock reentry it's not guaranteed that a synchronised method will issue a flush
         //if (opCode.isCall() &&
         //    !node->hasUnresolvedSymbolReference() &&
         //    node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isSynchronised())
         //   nodeHasSync = true;
         }

      bool notAnalyzedSync = true;
      FlushCandidate *flushCandidate;
      for (flushCandidate = _flushCandidates->getFirst(); flushCandidate; flushCandidate = flushCandidate->getNext())
      //for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
         {
         candidate = getCandidate(_candidates, flushCandidate);
         if (!candidate)
            continue;

         if (child &&
             ((_escapeAnalysis &&
               _escapeAnalysis->usesValueNumber(candidate, valueNumber)) ||
              !_escapeAnalysis))
               {
               if (!dependencies)
                  {
                  _allocationInfo->reset(candidate->_index);
                  TR_DependentAllocations *deps = NULL;
                  if (_escapeAnalysis)
                     deps = getDependentAllocationsFor(candidate, &(_escapeAnalysis->_dependentAllocations));
                  else
                     deps = getDependentAllocationsFor(candidate, &_dependentAllocations);

                  if (deps)
                     {
                     ListIterator<Candidate> candIt(deps->getDependentAllocations());
                     for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                        _allocationInfo->reset(dependentCandidate->_index);
                     }
                  }
               else
                  dependencies->addDependentAllocation(candidate);
               }

         if (((node == flushCandidate->getFlush()->getNode()) &&
              candidate->_flushRequired) ||
             (nodeHasSync &&
              notAnalyzedSync))
            {
            notAnalyzedSync = false;
            if (trace())
               {
               traceMsg(comp(), "\nConsidering Flush %p for allocation %p (index %d)\n", flushCandidate->getFlush()->getNode(), candidate->_node, candidate->_index);
               if (nodeHasSync)
                  {
                  traceMsg(comp(), "Also analyzing for nodeHasSync node %p; nodeHasVolatile %d\n", node, nodeHasVolatile);
                  }
               traceMsg(comp(), "Allocation info at this stage : \n");
               _allocationInfo->print(comp());
               traceMsg(comp(), "\n");
               }

            _temp->empty();
            TR_BitVectorIterator allocIt(*_allocationInfo);
            while (allocIt.hasMoreElements())
               {
               int32_t nextAlloc = allocIt.getNextElement();
               //if (nextAlloc == candidate->_index)
               //   continue;

               //if (trace())
               //          {
               //          traceMsg(comp(), "nextAlloc %d\n", nextAlloc);
               //          printf("nextAlloc %d\n", nextAlloc);
               //          fflush(stdout);
               //          }

               bool nextAllocCanReach = true;
               Candidate *reachingCandidate = NULL;
               FlushCandidate *reachingFlushCandidate = NULL;
               for (reachingFlushCandidate = _flushCandidates->getFirst(); reachingFlushCandidate; reachingFlushCandidate = reachingFlushCandidate->getNext())
               //for (reachingCandidate = _candidates->getFirst(); reachingCandidate; reachingCandidate = reachingCandidate->getNext())
                  {
                  if (reachingFlushCandidate->isOptimallyPlaced() ||
                     reachingFlushCandidate->getFlush()->getNode()->getAllocation() == NULL ||
                     reachingFlushCandidate->getBlockNum() != currBlock->getNumber())
                     continue;
                  reachingCandidate = getCandidate(_candidates, reachingFlushCandidate);
                  if (!reachingCandidate)
                     continue;

                  if ((reachingCandidate->_index == nextAlloc) &&
                      (reachingFlushCandidate != flushCandidate) &&
                      reachingFlushCandidate->getFlush() &&
                      visited.contains(reachingFlushCandidate->getFlush()->getNode()))
                     {
                     ListIterator<Candidate> candIt(&reachingCandidate->_flushMovedFrom);
                     for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                        {
                        if (!_allocationInfo->get(dependentCandidate->_index))
                           {
                           nextAllocCanReach = false;
                           break;
                           }
                        }
                     break;
                     }
                  }

               // If reachingFlushCandidate is already placed in the best possible location (above a volatile access) make sure we don't attempt to moved it
               if (nodeHasVolatile && reachingFlushCandidate && reachingFlushCandidate->getFlush()->getNode() == tt->getPrevTreeTop()->getNode())
                  {
                  reachingFlushCandidate->setOptimallyPlaced(true);
                  traceMsg(comp(), "AF %p is now marked as Optimally Placed!\n", reachingFlushCandidate->getFlush()->getNode() );
                  //fprintf( stderr, "AF %p is now marked as Optimally Placed!\n", reachingFlushCandidate->getFlush()->getNode() );
                  }

               // Skip to next allocation if we don't have a candidate, or the reachingFlushCandidate is right were we want it already
               if (!reachingFlushCandidate || !reachingCandidate || (nodeHasSync && reachingFlushCandidate->getFlush()->getNode() == tt->getPrevTreeTop()->getNode()))
                  continue;

               if (nextAllocCanReach)
                  {
                  reachingCandidate->_flushRequired = false;

                  TR::TreeTop *flushTree = reachingFlushCandidate->getFlush();
                  TR::TreeTop *prevTree = flushTree->getPrevTreeTop();
                  TR::TreeTop *nextTree = flushTree->getNextTreeTop();
                  if ((prevTree->getNextTreeTop() == flushTree) &&
                        (nextTree->getPrevTreeTop() == flushTree))
                     {
                     if (flushTree->getNode()->getOpCodeValue() == TR::allocationFence)
                        {
                        TR_ASSERT_FATAL(flushTree->getNode()->getAllocation()!=NULL, "localEA: Attempt to omit sync on a fence (%p) with allocation already set to ALL.\n", flushTree->getNode());
                        flushTree->getNode()->setOmitSync(true);
                        if (trace())
                           traceMsg(comp(),"(local) Setting AF node %p for allocation %p to omitsync\n", flushTree->getNode(), flushTree->getNode()->getAllocation() );
                        flushTree->getNode()->setAllocation(NULL);
                        }
                     //prevTree->join(nextTree);
                     }

                  //if (trace())
                  //   {
                  //   traceMsg(comp(), "1reaching candidate %p index %d does not need Flush\n", reachingCandidate, reachingCandidate->_index);
                  //   }

                  if (!nodeHasSync)
                     {
                     if (reachingCandidate != candidate)
                        {
                        if (!_temp->get(reachingCandidate->_index))
                           {
                           _temp->set(reachingCandidate->_index);
                           candidate->_flushMovedFrom.add(reachingCandidate);
                           }

                        ListIterator<Candidate> candIt(&reachingCandidate->_flushMovedFrom);
                        for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                           {
                           if (!_temp->get(dependentCandidate->_index))
                              {
                              _temp->set(dependentCandidate->_index);
                              candidate->_flushMovedFrom.add(dependentCandidate);
                              }
                           }
                        }
                     }
                  else
                     {
                     if (tt->getPrevTreeTop()->getNode()->getOpCodeValue() == TR::allocationFence)
                        {
                        TR::Node *afNode = tt->getPrevTreeTop()->getNode();

                        if (afNode->canOmitSync())
                           {
                           afNode->setAllocation(candidate->_node);
                           afNode->setOmitSync(false);

                           if (trace())
                              {
                              traceMsg(comp(), "(local) Restoring AF node %p for allocation %p above node %p.\n", afNode, candidate->_node, tt->getNode());
                              }
                           }
                        else
                           {
                           afNode->setAllocation(NULL); // Existing AllocationFence is now needed by more then one allocation
                           if (trace())
                              {
                              traceMsg(comp(), "(local) Setting AF node %p allocation to ALL above %p.\n", afNode, tt->getNode() );
                              }
                           }
                        }
                     else
                        {
                        // Insert an AllocationFence above the monExit/volatile-access. CodeGen can optimize the fence & monExit/volatile-access sequence
                        // so that only one flush instruction is executed. With lock-reservation or lock-reentry it's possible for a monExit to skip the
                        // flush so this AllocationFence will ensure correct behaviour. In addition, if the monExit/volatile-access happens to be
                        // moved/removed later in the compile, this AllocationFence will ensure a flush instruction is generated.
                        TR::Node *afNode = TR::Node::createAllocationFence(candidate->_node, candidate->_node);
                        tt->insertBefore(TR::TreeTop::create(comp(), afNode, NULL, NULL));
                        if (trace())
                           traceMsg(comp(), "(local) Inserted AF node %p for candidate %p above node %p (%s).\n", afNode, candidate->_node, node, node->getOpCode().getName() );
                        //fprintf( stderr, "(local) Inserted AF node %p above node %p (%s).\n", afNode, node, node->getOpCode().getName() );
                        }
                     }

                  if (trace())
                     {
                     if (!nodeHasSync)
                        {
                        traceMsg(comp(), "Flush for current allocation %d is post dominated by (and moved to) Flush for allocation %d\n", reachingCandidate->_index, candidate->_index);
                        //printf("Moved flush for allocation %p to allocation %p locally in method %s\n", reachingCandidate->_node, candidate->_node, comp()->signature());
                        }
                     else
                        {
                        traceMsg(comp(), "Flush for current allocation %d is post dominated by (and moved to) sync in the same block\n", reachingCandidate->_index);
                        //printf("Moved flush for allocation %p to real sync node %p locally in method %s\n", reachingCandidate->_node, node, comp()->signature());
                        }

                     //fflush(stdout);
                     }
                  }
               }

            if (!nodeHasSync)
               _allocationInfo->set(candidate->_index);
            }

         //if (node == candidate->_node)
         //   _allocationInfo->set(candidate->_index);
         }
      }
   else
      {
        //if (_escapeAnalysis)
         {
         int32_t firstArgIndex = node->getFirstArgumentIndex();
         for (int32_t arg = firstArgIndex; arg < node->getNumChildren(); arg++)
            {
            TR::Node *child = node->getChild(arg);
            int32_t valueNumber = -1;
            if (_escapeAnalysis)
               valueNumber = _escapeAnalysis->_valueNumberInfo->getValueNumber(child);
            Candidate *candidate;
            for (candidate = _candidates->getFirst(); candidate; candidate = candidate->getNext())
               {
               if ((_escapeAnalysis &&
                    _escapeAnalysis->usesValueNumber(candidate, valueNumber)) ||
                   !_escapeAnalysis)
                  {
                  _allocationInfo->reset(candidate->_index);

                  TR_DependentAllocations *deps = NULL;
                  if (_escapeAnalysis)
                     deps = getDependentAllocationsFor(candidate, &(_escapeAnalysis->_dependentAllocations));
                  else
                     deps = getDependentAllocationsFor(candidate, &_dependentAllocations);

                  if (deps)
                     {
                     ListIterator<Candidate> candIt(deps->getDependentAllocations());
                     for (Candidate *dependentCandidate = candIt.getFirst(); dependentCandidate; dependentCandidate = candIt.getNext())
                        _allocationInfo->reset(dependentCandidate->_index);
                     }
                  }
               }
            }
         }
      }

   int32_t i = 0;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);

      if (!examineNode(child, tt, currBlock, visited))
         return false;
      }

   return true;
   }
