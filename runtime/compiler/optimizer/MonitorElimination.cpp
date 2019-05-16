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

#include "optimizer/MonitorElimination.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/arrayof.h"
#include "cs2/bitvectr.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/InterProceduralAnalyzer.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "ras/LogTracer.hpp"

class TR_OpaqueClassBlock;

#define OPT_DETAILS "O^O MONITOR ELIMINATION: "

#define DO_THE_REMOVAL 1


class TR_MonitorPath : public TR_Link<TR_MonitorPath>
   {
   public:
   TR_MonitorPath(TR::Block *b, TR::TreeTop *t = NULL)
      {
      if (t == NULL)
         t = b->getFirstRealTreeTop();
      _block = b;
      _treeTop = t;
      }

   TR::Block *getBlock()       {return _block;}
   TR::TreeTop  *getTreeTop()  {return _treeTop;}
   TR::Node  *getNode()        {return _treeTop->getNode();}

   private:

   TR::Block *_block;
   TR::TreeTop *_treeTop;
   };

class TR_ActiveMonitor
   {
   public:
   TR_ALLOC(TR_Memory::MonitorElimination)

   TR_ActiveMonitor(TR::Compilation * c, TR::TreeTop *treeTop, int32_t numBlocks, TR_ActiveMonitor *container, bool t)
      : _comp(c), _monitorTree(treeTop),
        _currentBlocksSeen(numBlocks, c->trMemory(), stackAlloc),
        _outerBlocksSeen(numBlocks, c->trMemory(), stackAlloc),
        _innerBlocksSeen(numBlocks, c->trMemory(), stackAlloc),
        _partialExitBlocksSeen(numBlocks, c->trMemory(), stackAlloc),
        _redundant(false),
        _readMonitor(true),
        _exitTrees(c->trMemory()),
        _osrGuards(c->trMemory()),
        _monitorObject(0),
        _trace(t),
       _tmCandidate(false),
       _numberOfTreeTopsInsideMonitor(0),
       _containsCalls(false),
       _containsTraps(false),
       _numberOfLoads(0),
       _numberOfStores(0)

      {
      if (container)
         {
         _outerBlocksSeen |= container->_outerBlocksSeen;
         _outerBlocksSeen |= container->_currentBlocksSeen;
         }
      if (trace() && treeTop)
         traceMsg(c, "Adding new monitor [%p]\n", getMonitorNode());
      }

   TR::Compilation * comp() { return _comp; }

   bool trace() { return _trace; }

   TR::Node *getTreeTopNode()
      {
      if (!_monitorTree)
         return NULL;
      return _monitorTree->getNode();
      }

   TR::Node *getMonitorNode()
      {
      if (!_monitorTree)
         return NULL;
      TR::Node *node = _monitorTree->getNode();
      if (node->getOpCodeValue() == TR::NULLCHK ||
          node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();
      return node;
      }

   TR_OpaqueClassBlock * getMonitorClass()
      {
      TR::Node * monitorNode = getMonitorNode();
      return monitorNode ? monitorNode->getMonitorClass(comp()->getCurrentMethod()) : 0;
      }

   bool    isOuterScope()                            {return _monitorTree == NULL;}
   void    setBlockSeen(int32_t blockNum)            {_currentBlocksSeen.set(blockNum);}
   int32_t isBlockSeen(int32_t blockNum)             {return (_currentBlocksSeen.get(blockNum) || _innerBlocksSeen.get(blockNum) || _partialExitBlocksSeen.get(blockNum));}
   int32_t isBlockSeenInContainers(int32_t blockNum) {return _outerBlocksSeen.get(blockNum);}
   int32_t isPartialExitBlockSeen(int32_t blockNum)             {return (_partialExitBlocksSeen.get(blockNum));}

   TR_MonitorPath *getNextPath()                 {return _paths.pop();}
   bool            hasMorePaths()                {return !_paths.isEmpty();}

   void addPath(TR_MonitorPath *path)
      {
      _paths.add(path);
      _currentBlocksSeen.set(path->getBlock()->getNumber());
      if (trace())
         traceMsg(comp(), "Adding path [%p] in block_%d to monitor [%p]\n", path->getNode(), path->getBlock()->getNumber(), getMonitorNode());
      }

   void addPartialExitPath(TR_MonitorPath *path)
      {
      _paths.add(path);
      _partialExitBlocksSeen.set(path->getBlock()->getNumber());
      if (trace())
         traceMsg(comp(), "Adding partial exit path [%p] in block_%d to monitor [%p]\n", path->getNode(), path->getBlock()->getNumber(), getMonitorNode());
      }

   TR_ScratchList<TR::TreeTop> & getExitTrees() { return _exitTrees; }
   TR_ScratchList<TR::TreeTop> & getOSRGuards() { return _osrGuards; }

   void setMonitorObject(TR::TreeTop* treeTop) { _monitorObject = treeTop; }
   TR::TreeTop *getMonitorObject() { return _monitorObject; }

   void setMonitorTree(TR::TreeTop* treeTop) {_monitorTree = treeTop;}
   TR::TreeTop *getMonitorTree()             {return _monitorTree;}

   void setNotRedundant() { _redundant = false; }
   void setRedundant() { _redundant = true; }
   bool getRedundant() { return _redundant; }

   void setReadMonitor(bool b) { _readMonitor = b; }
   bool isReadMonitor() { return _readMonitor; }

   void exitMonitor(TR_ActiveMonitor *container)
      {
      if (container)
         container->_innerBlocksSeen |= _currentBlocksSeen;
      if (trace())
         traceMsg(comp(), "Found all exits for monitor [%p]\n", getMonitorNode());
      }


   void setTMCandidate() { _tmCandidate = true; }
   bool isTMCandidate() { return _tmCandidate; }
   void setNotTMCandidate() { _tmCandidate = false; }
   private:

   TR::Compilation *            _comp;
   TR::TreeTop                 *_monitorTree;
   TR::TreeTop                 *_monitorObject;
   TR_BitVector                _currentBlocksSeen;
   TR_BitVector                _outerBlocksSeen;
   TR_BitVector                _innerBlocksSeen;
   TR_BitVector                _partialExitBlocksSeen;
   TR_LinkHead<TR_MonitorPath> _paths;
   TR_ScratchList<TR::TreeTop>  _exitTrees;
   TR_ScratchList<TR::TreeTop>  _osrGuards;
   bool                        _redundant;
   bool                        _readMonitor;
   bool                        _trace;


   // TM Variables

   bool                         _tmCandidate;

   // not sure how useful this stuff is.
   // The problems with TM seem to be the following
   // 1) Transactions too close to each other (dynamically of course).  Ie you exit one transaction and a tbegin is too close by.
   // 2) Some threads are locking, and some are using TLE.  This is caused by some compiles not doing TLE on the same monitor
   //
   // Further info:
   // 1) Do we profile monitor object types?
   // 2) I think we want high false contention scenarios.  This allows TLE to work nicely
   // 3) No contention (ie single thread using a lock, or there's only one thread ever trying to acquire the lock) is bad.  There's a 3-5 cycle penalty for Tbegin and compareandswap is 1 cycle
   public:
   int32_t                      _numberOfTreeTopsInsideMonitor;
   bool                         _containsCalls;
   bool                         _containsTraps;
   int32_t                      _numberOfLoads;
   int32_t                      _numberOfStores;





   };


class TR_RecursionState
   {
   public:
   TR_ALLOC(TR_Memory::MonitorElimination)

   TR_RecursionState(TR::CFGNode *pred, TR::CFGNode *block, uint8_t state)
      : _pred(pred),
        _block(block),
        _state(state)
      {
      }

   TR::CFGNode    *getPred()  {return _pred;}
   void        setPred(TR::CFGNode *pred)  {_pred = pred;}
   TR::CFGNode    *getBlock()    {return _block;}
   void        setBlock(TR::CFGNode *block) {_block = block;}
   TR::CFGNode    *getSucc()  {return _succ;}
   void        setSucc(TR::CFGNode *succ)  {_succ = succ;}
   uint8_t     getState()  {return _state;}
   void        setState(uint8_t state)  {_state = state;}

   private:

   union {
      TR::CFGNode *_pred;
      TR::CFGNode *_succ;
      };

   TR::CFGNode *_block;
   uint8_t _state;
   };



TR::MonitorElimination::MonitorElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
   _alreadyClonedNodes(manager->trMemory()),
   _alreadyClonedRhs(manager->trMemory()),
   _specialBlockInfo(manager->trMemory()),
   _coarsenedMonitorsInfo(manager->trMemory()),
   _monentEdges(manager->trMemory()),
   _monexitEdges(manager->trMemory()),
   _cfgBackEdges(manager->trMemory()),
   _splitBlocks(manager->trMemory()),
   _nullTestBlocks(manager->trMemory()),
   _monitors(manager->trMemory()),
   _hasTMOpportunities(false),
   _tracer(manager->comp(), this)
   //,
   //_tmCandidates(trMemory())
   {
   requestOpt(OMR::redundantMonitorElimination);

   if(manager->comp()->getOption(TR_DebugRedundantMonitorElimination))
      {
      _tracer.setTraceLevelToDebug();
      setTrace(true);   // want monitorelimination trace statements when debug trace is set

      traceMsg(comp(),"setting trace to true.  trace now returns %d\n",trace());
      }

   }

int32_t TR::MonitorElimination::perform()
   {
   if (comp()->getOption(TR_DisableMonitorOpts))
      {
      if (trace())
         traceMsg(comp(), "Monitor optimizations explicitly disabled\n");
      return 0;
      }

   if (comp()->getOSRMode() == TR::involuntaryOSR)
      return 0;

   // initialize single threaded opts even if there aren't monitors since it also removed
   // virtual guards.
   //
   _invalidateUseDefInfo = false;
   _invalidateValueNumberInfo = false;
   _invalidateAliasSets = false;

   //_trace = 1;

   if (!comp()->getMethodSymbol()->mayContainMonitors())
      return 1;


   if (trace())
      {
      traceMsg(comp(), "Starting Monitor Elimination for %s\n", comp()->signature());
      comp()->dumpMethodTrees("Trees before Monitor Elimination");
      }

   TR_ValueNumberInfo *valueNumberInfo = optimizer()->getValueNumberInfo();
   if (valueNumberInfo == NULL)
      {
      if (trace())
         traceMsg(comp(), "Can't do Monitor Elimination, no value number information\n");
      return 0;
      }

   if(!firstPass())
      {
      _alreadyClonedNodes.init();
      _alreadyClonedRhs.init(),
      _specialBlockInfo.init(),
      _coarsenedMonitorsInfo.init(),
      _monentEdges.init();
      _monexitEdges.init();
      _cfgBackEdges.init();
      _splitBlocks.init();
      _nullTestBlocks.init();
      _monitors.init();
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   comp()->incVisitCount();

   TR::CFG *cfg = comp()->getFlowGraph();
   _numBlocks = cfg->getNextNodeNumber();

   _monitorStack = new (trStackMemory()) TR_Stack<TR_ActiveMonitor*> (trMemory(), 8, false, stackAlloc);

   // Create an active monitor entry to represent the state outside the scope of
   // any monitor, and prime it with the entry block information
   //
   TR_ActiveMonitor *outerScope = new (trStackMemory()) TR_ActiveMonitor(comp(), NULL, _numBlocks, NULL, trace());
   TR_SuccessorIterator succs(cfg->getStart());
   for (TR::CFGEdge *edge = succs.getFirst(); edge; edge = succs.getNext())
      {
      TR::Block *block = toBlock(edge->getTo());
      if (block->getEntry())
         {
         TR_MonitorPath *newPath = new (trStackMemory()) TR_MonitorPath(block);
         outerScope->addPath(newPath);
         }
      }

   _monitorStack->push(outerScope);

   bool attemptRedundantMonitors = true;
   static bool disableOSRwithTM = feGetEnv("TR_disableOSRwithTM") ? true: false;
   if (comp()->getOption(TR_EnableOSR) && disableOSRwithTM)
      {
      if (trace())
         traceMsg(comp(), "Cannot remove redundant monitors: OSR disabled with TM\n");
      attemptRedundantMonitors = false;
      }

   if (comp()->getOSRMode() == TR::voluntaryOSR && comp()->getOption(TR_DisableLiveMonitorMetadata))
      {
      if (trace())
         traceMsg(comp(), "Cannot remove redundant monitors: voluntary OSR requires the monitor object stores from live monitor metadata\n");
      attemptRedundantMonitors = false;
      }

   if (attemptRedundantMonitors && findRedundantMonitors())
      {
      if (trace())
         traceMsg(comp(),"findRedundantMonitors returned true.  About to remove Redundant Monitors\n");
      removeRedundantMonitors();

#ifndef PUBLIC_BUILD
      /* enable TLE by default on supported HW for now */
      if(!comp()->getOption(TR_DisableTLE) && comp()->cg()->getSupportsTLE())
         {
         if (trace())
            traceMsg(comp(),"findRedundantMonitors returned true. about to check for TM candidates\n");

         if(evaluateMonitorsForTMCandidates())
            {
               if (trace())
                  traceMsg(comp(),"evaluateMonitorsForTMCandidates returned true. firstPass = %d numPassesCompleted = %d\n", firstPass(), manager()->numPassesCompleted());
               static const char *doTMInFirstPass = feGetEnv("TR_doTMInFirstPass");
               if(!firstPass() || doTMInFirstPass)
                  transformMonitorsIntoTMRegions();
            }
         }
#endif

      }
   else
      {
      dumpOptDetails(comp(), "Bad monitor structure found, abandoning monitor elimination\n");
      //if (debug("traceBadMonitorStructure"))
         traceMsg(comp(), "Bad monitor structure found while compiling %s\n", comp()->signature());
      }

   // static const char *ifThenReadMonitors = feGetEnv("TR_IfThenReadMonitors");

   if (comp()->cg()->getSupportsReadOnlyLocks())
      tagReadMonitors();

   if (!comp()->getOption(TR_DisableMonitorCoarsening))
      coarsenMonitorRanges();

   if (comp()->cg()->getSupportsReadOnlyLocks())
      transformIntoReadMonitor();

   if (_invalidateUseDefInfo)
      optimizer()->setUseDefInfo(NULL);
   if (_invalidateValueNumberInfo)
      optimizer()->setValueNumberInfo(NULL);
   if (_invalidateAliasSets)
      optimizer()->setAliasSetsAreValid(false);

   } // scope of the stack memory region

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after Monitor Elimination");
      traceMsg(comp(), "Ending Monitor Elimination\n");

      if (firstPass() && !_hasTMOpportunities)
         traceMsg(comp(), "Not doing second monitor elimination run because there are no TM opportunities\n");
      }

   manager()->incNumPassesCompleted();
   return 1; // actual cost
   }

const char *
TR::MonitorElimination::optDetailString() const throw()
   {
   return "O^O MONITOR ELIMINATION: ";
   }

// Find redundant monitors and put them into the redundant monitor list
//
// Return "false" if any unexpected monitor structure found
//
bool TR::MonitorElimination::findRedundantMonitors()
   {
   while (!_monitorStack->isEmpty())
      {
      // Look at the most recent monitor in the stack
      //
      int32_t stackIndex          = _monitorStack->topIndex();
      TR_ActiveMonitor *monitor   = _monitorStack->top();
      TR_ActiveMonitor *container = stackIndex ? _monitorStack->element(stackIndex-1) : NULL;

      // Determine count to use to eliminate calls inside blocks which should stop
      // TM transformations
      uint32_t maxColdBlockCount = MAX_COLD_BLOCK_COUNT; //value = 5
      uint32_t maxWarmBlockCount = MAX_WARM_BLOCK_COUNT; //value = 1000
      bool useWarm = true; //determines which block count to use

      uint32_t minBlockCount = 15 ; //useWarm ? maxWarmBlockCount : maxColdBlockCount;

      // Look at one of the paths for this monitor. If there are none, we are
      // finished with this monitor.
      //
      TR_MonitorPath *path = monitor->getNextPath();
      if (path == NULL)
         {
         _monitorStack->pop();
         monitor->exitMonitor(container);
         continue;
         }

      TR::Block *block = path->getBlock();
      TR::TreeTop *exitTree = block->getExit();

      if (trace())
         traceMsg(comp(), "Examining path [%p] in block_%d for monitor [%p]\n", path->getNode(), path->getBlock()->getNumber(), monitor->getMonitorNode());

      if (block->isOSRInduceBlock())
         {
         // Skip this block
         if (trace())
            traceMsg(comp(), "Found OSR induce block %d for monitor [%p]\n", block->getNumber(), monitor->getMonitorNode());

         // Add it to all monitors on the stack
         addOSRGuard(block->getEntry());
         continue;
         }

      // Look for any monitor entry or monitor exit nodes still left to be
      // processed in this block
      //
      uint32_t exceptionsRaised = 0;
      uint32_t exceptionsRaisedTillMonexit = 0;
      TR::TreeTop *treeTop;
      TR::TreeTop *syncMethodTemp = NULL;
      bool multipleTempsForMonitor = false;
      for (treeTop = path->getTreeTop(); treeTop != exitTree; treeTop = treeTop->getNextTreeTop())
         {
         TR::Node *node = treeTop->getNode();

         // Ignore the exception raised from the hoisted nullchk on the object being locked
         // This nullchk was inserted by the optimizer and we know that the monitor structure
         // is correct in the catch-all block
         //
         //
         //

         //Transactional Memory: possible suggested heuristic spot
         monitor->_numberOfTreeTopsInsideMonitor++;

         // this section is to deal with the fact that there's a bug in the s390 linux kernel which prevent traps from working correctly in TM
         if (!comp()->cg()->supportsTrapsInTMRegion())
            {
               if (node->getOpCode().canRaiseException())
                  {
                  // only certain exceptions are implicit on s390 linux.
                  if(node->getOpCodeValue() == TR::ArrayCopyBNDCHK || node->getOpCodeValue() ==  TR::BNDCHKwithSpineCHK ||
                     node->getOpCodeValue() == TR::BNDCHK || node->getOpCodeValue() == TR::NULLCHK || node->getOpCodeValue() == TR::DIVCHK )
                     {
                     debugTrace(tracer(),"Potential TM Region Contains an implicit exception at node %p ",node);
                     monitor->_containsTraps = true;
                     }
                  if (node->getOpCodeValue() == TR::BCDCHK)
                     {
                     debugTrace(tracer(),"Potential TM Region Contains an implicit exception at node %p ",node);
                     monitor->_containsTraps = true;
                     }
                  }
            }

         if (node->getOpCode().isCall() || (node->getNumChildren() > 0 &&  node->getFirstChild()->getOpCode().isCall()))
            {
            if (trace())
               traceMsg(comp(),"Monitor node %p has a call at %p in its monitor region\n",monitor->getMonitorNode(),node);

            if (!block->isCold() && block->getFrequency() >= minBlockCount ||
                (monitor->getMonitorTree() && (monitor->getMonitorTree()->getEnclosingBlock()->getNextBlock() == block)))
               {
               monitor->_containsCalls = true;
               if (trace())
                  traceMsg(comp(),"MJ Cannot eliminate block call Freq:%d, CCount:%d\n", block->getFrequency(), minBlockCount);
               }
            else
               {
               if (trace())
                  traceMsg(comp(),"MJ Cold block, do not stop TLE Freq:%d, isCold:%d \n", block->getFrequency(), block->isCold());
               }
            }


         // For live monitor meta data, the temp holding the monitored object must either be between the monent and the monexit or just
         // before the monent. It is necessary to find this store so that the monent can be repeated later if it is removed.
         // The temp store will only occur before the monent in the event that it is for a synchronized method.
         //
         if (node->getOpCode().isStoreDirect() && node->getOpCode().hasSymbolReference() && node->getNumChildren() > 0)
            {
            if (node->getSymbolReference()->holdsMonitoredObjectForSyncMethod())
               {
               if (multipleTempsForMonitor)
                  {
                  syncMethodTemp = NULL;
                  if (trace())
                     traceMsg(comp(), "Found another temp #%d holding monitored object for sync method, giving up\n", node->getSymbolReference()->getReferenceNumber());
                  }
               else
                  {
                  multipleTempsForMonitor = true;
                  syncMethodTemp = treeTop;
                  if (trace())
                     traceMsg(comp(), "Found temp #%d holding monitored object for sync method\n", node->getSymbolReference()->getReferenceNumber());
                  }
               }
            else if (node->getSymbolReference()->getSymbol()->holdsMonitoredObject() && _monitorStack->topIndex() > 0)
               {
               // This temp should match the monitor that is currently locked
               TR_ASSERT(!monitor->getMonitorObject(), "monitor should have only one object store within it");

               // The temp and the monitored object should be equivalent, otherwise the wrong temp was found
               if (monitor->getMonitorNode()->getNumChildren() > 0)
                  TR_ASSERT(optimizer()->areNodesEquivalent(node->getFirstChild(), monitor->getMonitorNode()->getFirstChild()), "object stored in temp and locked by monitor should be equivalent");

               monitor->setMonitorObject(treeTop);
               if (trace())
                  traceMsg(comp(), "Found temp #%d holding monitored object for monent [%p]\n", node->getSymbolReference()->getReferenceNumber(),
                     monitor->getMonitorNode());
               }
            }

         bool ignoreException = false;
         if (node->getOpCodeValue() == TR::NULLCHK)
            {
            if (node->getFirstChild()->getOpCodeValue() == TR::monent)
               ignoreException = true;
            else
               {
               TR::Node *next = treeTop->getNextRealTreeTop()->getNode();
               if (next->getOpCodeValue() == TR::monent &&
                   optimizer()->areNodesEquivalent(node->getNullCheckReference(), next->getFirstChild()))
                  ignoreException = true;
               }
            }
         else if (node->getOpCodeValue() == TR::monexitfence)
            {
            ignoreException = true;
            }

         // Accumulate exceptions that can be raised so that exception successors
         // can be added to the paths for this monitor.
         //
         if (!ignoreException)
            exceptionsRaised |= node->exceptionsRaised();

         if (node->getOpCodeValue() == TR::treetop ||
             node->getOpCodeValue() == TR::NULLCHK)
            {
            node = node->getFirstChild();
            }

         bool monitorNode = false;
         if (node->getOpCodeValue() == TR::monent)
            {
            // A new (nested) monitor found. Make sure we haven't seen it
            // before.
            //
            if (node->getVisitCount() == comp()->getVisitCount())
               {
               if (trace())
                  traceMsg(comp(), "Monitor enter [%p] found on more than one container path\n", node);
               //return false;
               }
            node->setVisitCount(comp()->getVisitCount());


            resetReadMonitors(_monitorStack->topIndex());

            // Add any exception paths for the current monitor
            //
            //if (!addExceptionPaths(monitor, block->getExceptionSuccessors(), exceptionsRaised))
            //   return false;

            // Create a new active monitor entry for the new scope.
            //
            TR_ActiveMonitor *newMonitor = new (trStackMemory()) TR_ActiveMonitor(comp(), treeTop, _numBlocks, monitor, trace());
            path = new (trStackMemory()) TR_MonitorPath(block, treeTop->getNextTreeTop());
            newMonitor->addPath(path);
            _monitorStack->push(newMonitor);
            checkRedundantMonitor();

            // If this is a synchronized method, the temp should have already been found
            if (node->isSyncMethodMonitor() && syncMethodTemp)
               {
               if (node->getNumChildren() > 0)
                  TR_ASSERT(optimizer()->areNodesEquivalent(syncMethodTemp->getNode()->getFirstChild(), node->getFirstChild()), "object stored in temp and locked by sync monitor should be equivalent");
               newMonitor->setMonitorObject(syncMethodTemp);
               }

            break;
            }

         if (node->getOpCodeValue() == TR::monexit)
            {
            resetReadMonitors(_monitorStack->topIndex()-1);

            // The end of this monitor's scope on this path. Make sure we
            // haven't seen it before.
            //
            if (node->getVisitCount() == comp()->getVisitCount())
               {
               resetReadMonitors(_monitorStack->topIndex());
               if (trace())
                  traceMsg(comp(), "Monitor exit [%p] found on more than one container path\n", node);
               //return false;
               }
            node->setVisitCount(comp()->getVisitCount());

            // Add any exception paths for the current monitor
            //
            if (exceptionsRaisedTillMonexit && !addExceptionPaths(monitor, block->getExceptionSuccessors(), exceptionsRaisedTillMonexit))
               {
               resetReadMonitors(_monitorStack->topIndex());
               return false;
               }

            if (container == NULL)
               {
               if (trace())
                  traceMsg(comp(), "Monitor exit [%p] found without a corresponding monitor enter\n", node);
               resetReadMonitors(_monitorStack->topIndex());
               return false;
               }

            if (trace())
               traceMsg(comp(), "Monitor exit found at [%p] for monitor [%p]\n", node, monitor->getMonitorNode());

            monitor->getExitTrees().add(treeTop);

            // Add the path to the containing monitor's list of paths
            //
            path = new (trStackMemory()) TR_MonitorPath(block, treeTop->getNextTreeTop());
            if (!container->isPartialExitBlockSeen(path->getBlock()->getNumber()))
               container->addPartialExitPath(path);
            break;
            }

         exceptionsRaisedTillMonexit |= exceptionsRaised;

         if ((node->getOpCodeValue() != TR::monexit) &&
             (node->getOpCodeValue() != TR::monent))
            {
            if ((node->exceptionsRaised() != 0) ||
                node->getOpCode().isStoreIndirect() ||
                (node->getOpCodeValue() == TR::asynccheck))
               {
               resetReadMonitors(_monitorStack->topIndex());
               }
            }
         }

      // If a monitor enter or exit was processed, the stack has now been set
      // up for the next thing to be processed
      //
      if (treeTop != exitTree)
         continue;

      // If no monitor enter or exit was found in the current block on this path
      // just add its successors to the path list for the monitor.
      //
      if (!addPaths(monitor, block->getSuccessors()))
         {
         resetReadMonitors(_monitorStack->topIndex());
         return false;
         }
      if (!addExceptionPaths(monitor, block->getExceptionSuccessors(), exceptionsRaised))
         {
         resetReadMonitors(_monitorStack->topIndex());
         return false;
         }

      /////// If no paths were found out of this block and this is a real monitor,
      /////// there is a missing monitor exit
      ///////
      /////if (!morePathsFound && container)
      /////   {
      /////   if (trace())
      /////      traceMsg(comp(), "No monitor exit for monitor enter at [%p]\n", monitor->getMonitorNode());
      /////   return false;
      /////   }
      }

   // All done
   //
   return true;
   }

bool TR::MonitorElimination::addPath(TR_ActiveMonitor *monitor, TR::Block *block)
   {
   if (block->getEntry())
      {
      // If we have seen this block before at some outer monitor level
      // there is some problem with the monitor structure
      //
      if (monitor->isBlockSeenInContainers(block->getNumber()))
         {
         if (trace())
            traceMsg(comp(), "Monitor enter [%p] loops back to containing monitor scope via block_%d\n", monitor->getMonitorNode(), block->getNumber());
         return false;
         }

      // If we have seen this block before within this monitor's scope,
      // just ignore it
      //
      if (monitor->isBlockSeen(block->getNumber()))
         return true;

      TR_MonitorPath *newPath = new (trStackMemory()) TR_MonitorPath(block);
      monitor->addPath(newPath);
      }
   return true;
   }


bool TR::MonitorElimination::addPathAfterSkippingIfNecessary(TR_ActiveMonitor *monitor, TR::Block *block)
   {
   TR::Node *lastNode = block->getLastRealTreeTop()->getNode();

   if (((lastNode->getOpCodeValue() == TR::ifacmpeq) || (lastNode->getOpCodeValue() == TR::ifacmpne)) &&
       ((lastNode->getSecondChild()->getOpCodeValue() == TR::aconst) &&
       (lastNode->getSecondChild()->getAddress() == 0)))
      {
      TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
      int32_t firstChildValueNumber = info->getValueNumber(lastNode->getFirstChild());
      int32_t monitorValueNumber = -1;
      TR::Node *monitorNode = monitor->getMonitorNode();
      if (monitorNode)
         monitorValueNumber = info->getValueNumber(monitorNode->getFirstChild());

       if (firstChildValueNumber == monitorValueNumber)
          {
          if (lastNode->getOpCodeValue() == TR::ifacmpeq)
             block = block->getNextBlock();
          else
             block = lastNode->getBranchDestination()->getNode()->getBlock();
          }
       }

    if (!addPath(monitor, block))
       return false;

    return true;
    }


bool TR::MonitorElimination::addPaths(TR_ActiveMonitor *monitor, TR::CFGEdgeList &paths)
   {
   for (auto edge = paths.begin(); edge != paths.end(); ++edge)
      {
      TR::Block *block = toBlock((*edge)->getTo());
      if (block->getEntry())
         {
         if (!addPathAfterSkippingIfNecessary(monitor, block))
            return false;
         }
      }
   return true;
   }

bool TR::MonitorElimination::addExceptionPaths(TR_ActiveMonitor *monitor, TR::CFGEdgeList &paths, uint32_t exceptionsRaised)
   {
   for (auto edge = paths.begin(); edge != paths.end(); ++edge)
      {
      TR::Block *block = toBlock((*edge)->getTo());
      if (block->getEntry())
         {
         if (block->canCatchExceptions(exceptionsRaised))
            {
            if (!addPathAfterSkippingIfNecessary(monitor, block))
               return false;
            }
         }
      }
   return true;
   }

void TR::MonitorElimination::checkRedundantMonitor()
   {
   TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
   TR_ActiveMonitor *currentMonitor = _monitorStack->top();
   _monitors.add(currentMonitor);
   TR::Node *monitorNode = currentMonitor->getMonitorNode();
   if (0 && monitorNode->isLocalObjectMonitor())
      {
      bool redundant = true;
      TR::TreeTop * exitTree;
      ListIterator<TR::TreeTop> exitTrees(&currentMonitor->getExitTrees());
      for (exitTree = exitTrees.getFirst(); exitTree && redundant; exitTree = exitTrees.getNext())
    {
    TR::Node *monexitNode = exitTree->getNode();
    if (monexitNode->getOpCodeValue() != TR::monexit)
      monexitNode = monexitNode->getFirstChild();
         TR_ASSERT((monexitNode->getOpCodeValue() == TR::monexit), "Should find a monexit node\n");
    if (!monexitNode->isLocalObjectMonitor())
       redundant = false;
    }

      if (redundant)
         currentMonitor->setRedundant();
      }
   else
      {
      int32_t valueNumber = info->getValueNumber(monitorNode->getFirstChild());
      for (int32_t i = _monitorStack->topIndex()-1; i > 0; --i)
         {
         TR_ActiveMonitor *containingMonitor = _monitorStack->element(i);
         if (valueNumber == info->getValueNumber(containingMonitor->getMonitorNode()->getFirstChild()))
            {
            currentMonitor->setRedundant();
            break;
            }
         }
      }
   }


void TR::MonitorElimination::removeRedundantMonitors()
   {
   TR_ActiveMonitor *monitor = NULL;
   ListIterator<TR_ActiveMonitor> it(&_monitors);
   for (monitor = it.getFirst(); monitor; monitor = it.getNext())
      {
      TR::Node *monitorNode = monitor->getMonitorNode();
      if (monitorNode->isLocalObjectMonitor())
         {
         bool redundant = true;
         TR::TreeTop * exitTree;
         ListIterator<TR::TreeTop> exitTrees(&monitor->getExitTrees());
         for (exitTree = exitTrees.getFirst(); exitTree && redundant; exitTree = exitTrees.getNext())
       {
       TR::Node *monexitNode = exitTree->getNode();
       if (monexitNode->getOpCodeValue() != TR::monexit)
          monexitNode = monexitNode->getFirstChild();
            TR_ASSERT((monexitNode->getOpCodeValue() == TR::monexit), "Should find a monexit node\n");
       if (!monexitNode->isLocalObjectMonitor())
          redundant = false;
       }

         if (redundant)
            monitor->setRedundant();
         }

      // If no monitor object was found and there are OSR guards within the monitor,
      // it cannot be removed
      if (monitor->getRedundant() && !monitor->getMonitorObject() && monitor->getOSRGuards().getSize() > 0)
         monitor->setNotRedundant();
      }

     ListIterator<TR_ActiveMonitor> it1(&_monitors);
     TR_ActiveMonitor *monenter = NULL;
     for (monenter = it1.getFirst(); monenter; monenter = it1.getNext())
        {
        if (monenter->getRedundant())
           {
           bool redundant = true;
           TR::TreeTop * exitTree;
           ListIterator<TR::TreeTop> exitTrees(&monenter->getExitTrees());
           for (exitTree = exitTrees.getFirst(); exitTree; exitTree = exitTrees.getNext())
         {
              ListIterator<TR_ActiveMonitor> it(&_monitors);
              for (TR_ActiveMonitor *monitor = it.getFirst(); monitor; monitor = it.getNext())
                 {
                 if (monitor->getExitTrees().find(exitTree))
                    {
                    if (!monitor->getRedundant())
                       {
                       redundant = false;
                       break;
                       }
                    }
                 }
              }

           if (!redundant)
              monenter->setNotRedundant();
           }
        }


   ListIterator<TR_ActiveMonitor> it2(&_monitors);
   for (monitor = it2.getFirst(); monitor; monitor = it2.getNext())
      {
      TR::Node *monitorNode = monitor->getMonitorNode();

      if (monitor->getRedundant())
         {
         rematMonitorEntry(monitor);
         removeMonitorNode(monitor->getTreeTopNode());

         ListIterator<TR::TreeTop> exitTrees(&monitor->getExitTrees());
         for (TR::TreeTop *exitTree = exitTrees.getFirst(); exitTree; exitTree = exitTrees.getNext())
            removeMonitorNode(exitTree->getNode());
         }
      }
   }

/*
 * When removing a monitor, for every OSR guard that exists within it,
 * the monitor entry should be repeated on the taken side of the guard.
 *
 * This method duplicates the monitor entry, removing any nullchecks or other
 * treetops, and uses the live monitor metadata temps to ensure its the same object
 * as before.
 */
void TR::MonitorElimination::rematMonitorEntry(TR_ActiveMonitor *monitor)
   {
   TR::Node *monitorNode = monitor->getMonitorNode();
   if (monitorNode->getOpCodeValue() != TR::monent)
      return;

   ListIterator<TR::TreeTop> osrGuards(&monitor->getOSRGuards());
   for (TR::TreeTop *osrGuard = osrGuards.getFirst(); osrGuard; osrGuard = osrGuards.getNext())
      {
      // Duplicate the monent, replacing its child with the meta live data temp
      TR::Node *load = TR::Node::createLoad(monitor->getMonitorObject()->getNode()->getSymbolReference());
      TR::Node *node = monitorNode->duplicateTree();
      if (node->getFirstChild())
         node->getFirstChild()->recursivelyDecReferenceCount();
      node->setAndIncChild(0, load);
      TR::TreeTop::create(comp(), osrGuard, node);
      }
   }

/*
 * Add the current guard to all monitors on the stack, as they will all contain
 * this transition point.
 */
void TR::MonitorElimination::addOSRGuard(TR::TreeTop *guard)
   {
   for (int32_t i = _monitorStack->topIndex(); i > 0; --i)
      {
      TR_ActiveMonitor *monitor = _monitorStack->element(i);
      monitor->getOSRGuards().add(guard);
      }
   }

#ifndef PUBLIC_BUILD

// returns true if  we find at least one monitor who is a candidate for TM
bool TR::MonitorElimination::evaluateMonitorsForTMCandidates()
   {



   debugTrace(tracer(),"TM:In evaluateMonitorsForTMCandidates.  Number of monitors to consider = %d\n",_monitors.getSize());


   static const char *p = feGetEnv("TM_MaxMonitors");
   int32_t maxmonitors = p ? atoi(p) : -1;
   int32_t monitorsadded=0;

   TR_ActiveMonitor *monitor = NULL;
   ListIterator<TR_ActiveMonitor> it(&_monitors);
   for (monitor = it.getFirst(); monitor; monitor = it.getNext())
      {
        // evaluate some conditions
        //
      if(monitor->getRedundant())
         continue;


      if(monitor->_containsCalls)
         {
         traceMsg(comp(), "TM: monitor at node %p contains calls. Not doing TM\n",monitor->getMonitorNode());
         continue;
         }

      if(!comp()->cg()->supportsTrapsInTMRegion() &&monitor->_containsTraps)
         {
         heuristicTrace(tracer(),"TM: monitor at node %p contains trap instructions and platform does not support it. Not doing TM",monitor->getMonitorNode());
         continue;
         }

      int32_t minTTs= TR::Options::_minimalNumberOfTreeTopsInsideTMMonitor;
      if (monitor->_numberOfTreeTopsInsideMonitor <= minTTs)
         {
        traceMsg(comp(), "TM: monitor at node %p only has %d TreeTops. Not doing TM\n",monitor->getMonitorNode(), minTTs);
         continue;
         }

      if(hasMultipleEntriesWithSameExit(monitor))
         {
         if(trace())
            traceMsg(comp(), "TM: monitor at node %p has multiple exits for a given entry (not supported yet. Not doing TM",monitor->getMonitorNode());
         continue;
         }

      if(maxmonitors <0 ||  monitorsadded < maxmonitors)
         {
         TR_OpaqueClassBlock * monClass = monitor->getMonitorClass();
         int32_t lwOffset = comp()->cg()->fej9()->getByteOffsetToLockword(monClass);
         // only do TLE for objects that have lockwords in object header
         if (lwOffset>0)
            {
            heuristicTrace(tracer(),"TM: setting monitor %p (node %p) to TLE candidate\n",monitor,monitor->getTreeTopNode() );
            //_tmCandidates.add(monitor);
            monitor->setTMCandidate();
            ++monitorsadded;
            _hasTMOpportunities = true;
            optimizer()->setRequestOptimization(OMR::redundantMonitorElimination);
            }
         }
      }
   // A second pass is needed in case we place to TM Regions close to one another
   it.reset();
   for (monitor = it.getFirst(); monitor; monitor = it.getNext())
      {
      debugTrace(tracer(),"TM: Checking monitor %p (node %p) is not near any other monitor regions\n",monitor,monitor->getTreeTopNode() );
      searchAndLabelNearbyMonitors(monitor);
      }

   return true;
   }



TR::SymbolReference* TR::MonitorElimination::createAndInsertTMRetryCounter(TR_ActiveMonitor *monitor)
   {
   // need to invalidate these because we are creating new load/stores
   _invalidateUseDefInfo = true;
   _invalidateValueNumberInfo = true;
   _invalidateAliasSets = true;

   TR::SymbolReference * tempSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);      //our temporary 't' variable

   int32_t retryCount = TR::Options::_TransactionalMemoryRetryCount;

   TR::Node * constNode = TR::Node::create(monitor->getMonitorNode(), TR::iconst, 0, retryCount);
   TR::Node * storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, constNode, tempSymRef);

   monitor->getMonitorTree()->insertBefore(TR::TreeTop::create(comp(),storeNode,NULL,NULL));

   traceMsg(comp(),"Created tempSymRef (%p) for temporary\n",tempSymRef);

   return tempSymRef;
   }

TR_Array<TR::Block *>* TR::MonitorElimination::createFailHandlerBlocks(TR_ActiveMonitor *monitor, TR::SymbolReference *tempSymRef, TR::Block *monitorblock, TR::Block *tstartblock)
   {

   //This array is used to store the first block and the last block of the fail handler. (In order)  THe rest of the handler should be self contained
   //This array is used to store important fail handler blocks.  element 0 -> persistent fail start, element 1 -> transient failure start, element 2 -> last block
   TR_Array<TR::Block *> *fhBlocksArray = new (comp()->trStackMemory()) TR_Array<TR::Block *>(comp()->trMemory(),3,true,stackAlloc);


   //creating persistent failure block

   TR::Node *resetTempNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(monitor->getMonitorNode(), TR::iconst, 0, 0), tempSymRef);

   TR::Block *persistfhBlock = TR::Block::createEmptyBlock(monitor->getMonitorNode(), comp(), 6);
   persistfhBlock->append(TR::TreeTop::create(comp(), resetTempNode, NULL, NULL));


   // New logic. I'm adding a goto at the end of the persistent fail block to point directly to the monitor block.  This should allow induction variable analysis to exclude the setting of the temp=0 from the loop, and make it a counted loop.

   persistfhBlock->append(TR::TreeTop::create(comp(), TR::Node::create(monitor->getMonitorNode(), TR::Goto, 0, monitorblock->getEntry())));



   //create transient failure block -- sub first then do compare

   TR::Node *monentif = TR::Node::createif(TR::ificmple,TR::Node::createWithSymRef(monitor->getMonitorNode(),TR::iload,0,tempSymRef),TR::Node::create(monitor->getMonitorNode(),TR::iconst,0,0),NULL);

   monentif->setByteCodeInfo(monitor->getMonitorNode()->getByteCodeInfo());
   monentif->setBranchDestination(monitor->getMonitorTree()->getEnclosingBlock()->getEntry());

   TR::Block *fhBlock = TR::Block::createEmptyBlock(monitor->getMonitorNode(),comp(),6);      // eww a magic number.  the node passed in is used to populate the node with the right bc info, etc.
   fhBlock->append(TR::TreeTop::create(comp(),monentif,NULL,NULL));

   TR::Block *check1 = TR::Block::createEmptyBlock(monitor->getMonitorNode(),comp(),6);

   TR::Node *subtree = TR::Node::createWithSymRef(TR::istore, 1, 1,
                        TR::Node::create(TR::isub, 2,
                           TR::Node::createWithSymRef(monitor->getMonitorNode(), TR::iload, 0, tempSymRef),
                           TR::Node::create(monitor->getMonitorNode(),TR::iconst, 0, 1))
                      , tempSymRef);

   check1->append(TR::TreeTop::create(comp(), subtree, NULL, NULL));

   //join the first block to the second

   persistfhBlock->getExit()->join(check1->getEntry());
   check1->getExit()->join(fhBlock->getEntry());

   TR::CFG *cfg = comp()->getFlowGraph();

   cfg->addNode(persistfhBlock);
   cfg->addNode(fhBlock);
   cfg->addNode(check1);

   cfg->addEdge(persistfhBlock,monitorblock);
   cfg->addEdge(check1,fhBlock);

   cfg->addEdge(fhBlock,tstartblock);                                            //FIXME: there's an implicit Assumption that check1 falls through to tstart
   cfg->addEdge(tstartblock,check1);
   cfg->addEdge(tstartblock,persistfhBlock);
   cfg->addEdge(fhBlock,monitor->getMonitorTree()->getEnclosingBlock());

   traceMsg(comp(),"Created fhBlock %d(%p)\n",fhBlock->getNumber(),fhBlock);
   traceMsg(comp(),"Created check1Block %d(%p)\n",check1->getNumber(),check1);

   fhBlocksArray->add(persistfhBlock);
   fhBlocksArray->add(check1);
   fhBlocksArray->add(fhBlock);
   return fhBlocksArray;
   }


bool TR::MonitorElimination::searchDownForOtherMonitorsInCurrentBlock(TR::TreeTop *tt, int32_t &size, TR::TreeTop *& monitorTT)
   {

   for (TR::TreeTop *it = tt ; it ; it = it->getNextTreeTop())
      {
      size++;

      TR::Node *node = it->getNode();
      if (node->getOpCodeValue() == TR::treetop ||node->getOpCodeValue() == TR::NULLCHK)
         {
         node = node->getFirstChild();
         }

 //     if(trace())
  //       traceMsg(comp(),"In single block search.  Considering node %p\n",node);

      if (node->getOpCodeValue() == TR::BBEnd)
         break;
      else if (node->getOpCodeValue() == TR::monent )
         {
         monitorTT = it;
         return true;
         }
      }
   return false;
   }

void addSuccessorsToBeProcessed(TR::Block *block, TR_Stack<TR::Block *> &nodesToBeEvaluated, int32_t depth,CS2::ArrayOf<int32_t, TR::Allocator> &nodeDepth )
   {
   for(auto succEdge = block->getSuccessors().begin() ; succEdge != block->getSuccessors().end(); ++succEdge)
      {
      TR::Block *succ = (*succEdge)->getTo()->asBlock();
      nodeDepth[succ->getNumber()] = depth;
      nodesToBeEvaluated.push(succ);
      }

   for(auto succEdge = block->getExceptionSuccessors().begin() ; succEdge != block->getExceptionSuccessors().end(); ++succEdge)
      {
      TR::Block *succ = (*succEdge)->getTo()->asBlock();
      nodeDepth[succ->getNumber()] = depth;
      nodesToBeEvaluated.push(succ);
      }
   }

bool TR::MonitorElimination::searchDownForOtherMonitorsInSuccessors(TR::TreeTop *tt,int32_t &size, TR::list<TR::TreeTop *> &closeMonitors, int32_t maxDepth, int32_t minNumberOfNodes)
   {
   TR_Stack<TR::Block *> nodesToBeEvaluated(comp()->trMemory());
   CS2::ArrayOf<int32_t, TR::Allocator> nodeDepth(comp()->getFlowGraph()->getNumberOfNodes(), comp()->allocator());
   CS2::ArrayOf<int32_t, TR::Allocator> numberOfNodesAtDepth(maxDepth, comp()->allocator());

   int32_t depth = 0;
   numberOfNodesAtDepth[depth] = size;

   if(trace())
      traceMsg(comp(), "Begun search down for monitors in successors  monexit at %p.\n Setting numberOfNodesAtDepth[%d] to %d\n",tt->getNode(),depth,size);

   TR::Block *currentBlock = tt->getEnclosingBlock();
   depth++;
   addSuccessorsToBeProcessed(currentBlock,nodesToBeEvaluated,depth,nodeDepth);



   do
      {
      currentBlock = nodesToBeEvaluated.pop();
      depth = nodeDepth[currentBlock->getNumber()];

      if(trace())
          traceMsg(comp(), "Considering block %d  depth %d\n",currentBlock->getNumber(),depth);


      int32_t currentBlockSize = 0;
      TR::TreeTop *monitorTT=0;
      if(searchDownForOtherMonitorsInCurrentBlock(currentBlock->getEntry(), currentBlockSize, monitorTT))
         {
         int32_t numberOfTTs=currentBlockSize;
         for(int32_t i=0; i<depth ; i++)
            {
            if(trace())
               traceMsg(comp(),"i = %d, nodeDepth[i] = %d\n",i,numberOfNodesAtDepth[i]);
            numberOfTTs+=numberOfNodesAtDepth[i];
            }
         if(trace())
              traceMsg(comp(), "Found a monitor %p numberOfTTs %d\n",monitorTT->getNode(),numberOfTTs);

         if (numberOfTTs < minNumberOfNodes )
            closeMonitors.push_back(monitorTT);
         }
      else if (depth < maxDepth)
         {
         if(trace())
           traceMsg(comp(), "Setting numberOfNodesAtDepth[%d] to currentBlockSize %d\n",depth,currentBlockSize);

         numberOfNodesAtDepth[depth] = currentBlockSize;
         depth++;
         addSuccessorsToBeProcessed(currentBlock,nodesToBeEvaluated,depth,nodeDepth);
         }


      } while (!nodesToBeEvaluated.isEmpty());

   if(closeMonitors.empty())
      return false;
   return true;
   }

TR_ActiveMonitor*
TR::MonitorElimination::findActiveMonitor(TR::TreeTop *tt)
   {
   TR_ActiveMonitor *monitor = NULL;
   ListIterator<TR_ActiveMonitor> it(&_monitors);
   for (monitor = it.getFirst(); monitor; monitor = it.getNext())
      {
      if(monitor->getMonitorTree() == tt)
         return monitor;
      }
   return 0;
   }

void TR::MonitorElimination::searchAndLabelNearbyMonitors(TR_ActiveMonitor *currentMonitor)
   {

   static const int32_t maxDepth = 4;           // this is the maxDepth of our CFG search.  Ie, this will limit how many blocks down we go visit for the current monitor
   static const int32_t minNumberOfNodes = comp()->cg()->getMinimumNumberOfNodesBetweenMonitorsForTLE();  // the number of nodes we want between a monexit and the next monent

   TR::CFG *cfg = comp()->getFlowGraph();

   if(trace())
      traceMsg(comp(),"Begun search for other nearby Monitors.  Active Monitor %p with Node %p\n",currentMonitor,currentMonitor->getMonitorNode());

   ListIterator<TR::TreeTop> treeIT (&currentMonitor->getExitTrees());
   TR::TreeTop *et = 0;
   for(et = treeIT.getCurrent() ; et ; et = treeIT.getNext())
      {
      if (trace())
         traceMsg(comp(), "Considering exit at node %p\n",et->getNode());

      int32_t numberOfTTs = 0;
      TR::TreeTop *monitorTT = 0;

      TR::list<TR::TreeTop *> closeMonitors(getTypedAllocator<TR::TreeTop*>(comp()->allocator()));

      if(searchDownForOtherMonitorsInCurrentBlock(et,numberOfTTs,monitorTT))       // found the closest monent in the current block
         {
         if (numberOfTTs < minNumberOfNodes )
            {
            TR_ActiveMonitor *monitor = findActiveMonitor(monitorTT);
            if (monitor)
               {
               if(trace())
                  traceMsg(comp(), "Setting Active monitor with monitorNode %p to NOT a TM Candidate because it's too close"
                                  " to previous TM Candidate with monexit %p\n",monitor->getMonitorNode(),et->getNode());
               monitor->setNotTMCandidate();
               }
            }
         }
      else if(searchDownForOtherMonitorsInSuccessors(et,numberOfTTs,closeMonitors,maxDepth,minNumberOfNodes))
         {
         for(auto iterator = closeMonitors.begin(); iterator != closeMonitors.end(); ++iterator)
            {
            TR::TreeTop *monitorTTinOtherBlock = *iterator;
            TR_ActiveMonitor *monitor = findActiveMonitor(monitorTTinOtherBlock);
            if (monitor)
               {
               if(trace())
                  traceMsg(comp(), "Setting Active monitor with monitorNode %p to NOT a TM Candidate because it's too close (in another block)"
                                  " to previous TM Candidate with monexit %p\n",monitor->getMonitorNode(),et->getNode());
               monitor->setNotTMCandidate();
               }
            }

         }
      }


   }

bool TR::MonitorElimination::hasMultipleEntriesWithSameExit(TR_ActiveMonitor *monitor)
   {
   // this goes and checks to see if the monexit is on another monenter path.  We will only perform tm if all monenter paths are tm candidates
  bool isCandidate = true;
  TR::TreeTop * exitTree1;
  ListIterator<TR::TreeTop> exitTrees1(&monitor->getExitTrees());
  for (exitTree1 = exitTrees1.getFirst(); exitTree1; exitTree1 = exitTrees1.getNext())
     {
     ListIterator<TR_ActiveMonitor> it1(&_monitors);
     for (TR_ActiveMonitor *monitor1 = it1.getFirst(); monitor1; monitor1 = it1.getNext())
        {
        if (monitor != monitor1 && monitor1->getExitTrees().find(exitTree1))
           {
            isCandidate = false; //doing this for now because I don't know how to share temporaries yet.
            break;
              if (!monitor1->isTMCandidate())
              {
              isCandidate = false;
              break;
              }
           }
        }
     }
   if (!isCandidate)
      {
      traceMsg(comp(), "TM:monitor %p at node %p is NOT a TM Candidate because some other monitor sharing the exit is not a TM Candidate\n",monitor,monitor->getTreeTopNode());
      monitor->setNotTMCandidate();
      return true;
      }
   return false;
   }

void TR::MonitorElimination::transformMonitorsIntoTMRegions()
   {


   debugTrace(tracer(),"TM:In transformMonitorsIntoTMRegions.\n");

   TR_ActiveMonitor *monitor = NULL;
   ListIterator<TR_ActiveMonitor> it(&_monitors);
   for (monitor = it.getFirst(); monitor; monitor = it.getNext())
      {


      debugTrace(tracer(),"TM:Considering monitor %p at node %p\n",monitor,monitor->getTreeTopNode());

      if (!monitor->isTMCandidate())
         {
         debugTrace(tracer(),"TM:monitor %p at node %p is NOT a TM Candidate\n",monitor,monitor->getTreeTopNode());
         continue;
         }


      heuristicTrace(tracer(),"TM:Monitor is a TM Candidate. Will attempt to transform region.\n");


      TR::CFG *cfg = comp()->getFlowGraph();
      cfg->setStructure(NULL);        // voiding out structure

      if (monitor->getTreeTopNode()->getOpCodeValue() == TR::NULLCHK)
         {
         // A SEGV in a transaction on X & Z will abort the transaction, on p it will core, therefore an explicit
         // NullChk is required on P and is better then allowing a number of SEGVs on X & Z when locking a NULL ref.
         // Rip out the NULLCHK from the monent tree and put it on it's own TT above the tstart
         debugTrace(tracer(),"monent TT node is NULLCHK!\n\tMoving NULLCHK to a new TT");
         TR::Node *nullChkNode = monitor->getTreeTopNode();
         TR::Node *monentNode = nullChkNode->getFirstChild();
         TR::Node *passThroughNode = TR::Node::create(nullChkNode, TR::PassThrough, 1, monentNode->getFirstChild());
         TR::TreeTop *nullchkTree = TR::TreeTop::create(comp(), nullChkNode);
         nullChkNode->setAndIncChild(0, passThroughNode);
         monitor->getMonitorTree()->insertBefore(nullchkTree);
         debugTrace(tracer(), "\tMoving monent to TT and dec ref count", nullchkTree, nullchkTree->getNode());
         monitor->getMonitorTree()->setNode(monentNode);
         TR_ASSERT(monentNode->getReferenceCount() == 1, "monent ref count is not 1");
         monentNode->decReferenceCount();
         }

      //Part 1: Create Temporary
      TR::SymbolReference *tempSymRef = createAndInsertTMRetryCounter(monitor);


      //Part 2: split the monitor block at the spot of the monitor. in the first half of the block, append a goto to the second half of the block.  insert the fail handler block in between these two.
      // We rely on block splitting to properly create temporaries for us.  We do this by adding a treetop with the monitor child above where we do the split.  It works out nicely :)

      TR::TreeTop *tempTree = TR::TreeTop::create(comp(),TR::Node::create(TR::treetop, 1, monitor->getMonitorNode()->getFirstChild()));
      monitor->getMonitorTree()->insertBefore(tempTree);

      TR::Block *monitorblock = monitor->getMonitorTree()->getEnclosingBlock()->split(monitor->getMonitorTree(),cfg,true,true);
      TR::Block *tophalfmonitorblock = monitorblock->getEntry()->getPrevTreeTop()->getEnclosingBlock();
      TR::Block *critSectStart = 0;

      debugTrace(tracer(),"After monitor split.getmonitornode = %p  monitor->getMonitorTree() = %p node = %p nexttt = %p nextt node = %p\n",monitor->getMonitorNode(),monitor->getMonitorTree(),monitor->getMonitorTree()->getNode(),monitor->getMonitorTree()->getNextTreeTop(),monitor->getMonitorTree()->getNextTreeTop()->getNode());
      debugTrace(tracer(),"tophalfmonitorblock = %p %d , exit tt = %p node %p\n",tophalfmonitorblock,tophalfmonitorblock->getNumber(),tophalfmonitorblock->getExit(),tophalfmonitorblock->getExit()->getNode());


      if (monitor->getMonitorTree()->getNextTreeTop()->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         traceMsg(comp(),"next treetop after monitor has node %p. monitorblock = %p. Not splitting\n",monitor->getMonitorTree()->getNextTreeTop()->getNode(),monitorblock->getNumber(),monitorblock);
         critSectStart = monitor->getMonitorTree()->getEnclosingBlock()->getExit()->getNextTreeTop()->getEnclosingBlock();
         }
      else
         {
         critSectStart = monitor->getMonitorTree()->getEnclosingBlock()->split(monitor->getMonitorTree()->getNextTreeTop(),cfg,true,true);
         traceMsg(comp(),"next treetop after monitor has node %p. critsectionblock = %d %p. splitting\n",monitor->getMonitorTree()->getNextTreeTop()->getNode(),critSectStart->getNumber(),critSectStart);
         }

      debugTrace(tracer(),"monitorblock = %d(%p) critSectStart = %d(%p)\n",monitorblock->getNumber(),monitorblock,critSectStart->getNumber(),critSectStart);


      //clone the monitor block to create the tstartblock.  put this newly created tstart block in the spot of the monitor
      //move the monitor block to the end of the il

      TR_BlockCloner cloner(cfg);
      TR::Block * tstartblock = cloner.cloneBlocks(monitorblock,monitorblock);


      debugTrace(tracer(),"monitor block = %d(%p)\n",monitorblock->getNumber(),monitorblock);
      debugTrace(tracer(),"tstartblock = %d(%p)\n",tstartblock->getNumber(),tstartblock);
      debugTrace(tracer(),"tpohalfmonitorblock = %d(%p)\n",tophalfmonitorblock->getNumber(),tophalfmonitorblock);

      //Part 3: Create FH Block
      TR_Array<TR::Block *> *fhBlocks = createFailHandlerBlocks(monitor,tempSymRef,monitorblock,tstartblock);

      monitorblock->getEntry()->getPrevTreeTop()->join(tstartblock->getEntry());
      tstartblock->getExit()->join(monitorblock->getExit()->getNextTreeTop());



      TR::TreeTop *test1 = comp()->getMethodSymbol()->getLastTreeTop();  //seeing if we get past this point
      traceMsg(comp(),"test1 =%p\n",test1);

      tophalfmonitorblock->append(TR::TreeTop::create(comp(),TR::Node::create(monitor->getMonitorNode(),TR::Goto,0,tstartblock->getEntry())));
      //Insert the fail handler
      tstartblock->getEntry()->getPrevTreeTop()->join((*fhBlocks)[0]->getEntry());
      (*fhBlocks)[2]->getExit()->join(tstartblock->getEntry());

      test1 = comp()->getMethodSymbol()->getLastTreeTop();  //seeing if we get past this point
      traceMsg(comp(),"test1 =%p\n",test1);

      //append monitor block to end of the trees and make it go to the critical section
      monitorblock->getExit()->setNextTreeTop(0);       // its the end of the cfg now
      monitorblock->getEntry()->setPrevTreeTop(0);


      TR::TreeTop *monitorgoto = TR::TreeTop::create(comp(),TR::Node::create(monitor->getMonitorNode(),TR::Goto,0,critSectStart->getEntry()));
      monitorblock->append(monitorgoto);

      test1 = comp()->getMethodSymbol()->getLastTreeTop();  //seeing if we get past this point
      debugTrace(tracer(),"test1 =%p\n",test1);

      comp()->getMethodSymbol()->getLastTreeTop()->join(monitorblock->getEntry());
      //swap the duplicate monitor node for a tstart. set the branch destination of hte tstart to the fail handler



      TR::Node *persistentFailNode = TR::Node::create(monitor->getMonitorNode(), TR::branch,0,(*fhBlocks)[0]->getEntry());
      TR::Node *transientFailNode = TR::Node::create(monitor->getMonitorNode(), TR::branch,0,(*fhBlocks)[1]->getEntry());
      TR::Node *fallThroughNode = TR::Node::create(monitor->getMonitorNode(), TR::branch,0,critSectStart->getEntry());
      TR::Node *monitorObject = TR::Node::copy(monitor->getMonitorNode()->getFirstChild());
      monitorObject->setReferenceCount(0);
      TR::Node *tstartNode = TR::Node::createWithRoomForFive(TR::tstart,  persistentFailNode, transientFailNode,fallThroughNode,monitorObject);
      if(monitor->getMonitorNode()->hasMonitorClassInNode())
        tstartNode->setMonitorClassInNode(monitor->getMonitorNode()->getMonitorClassInNode());
      else
        tstartNode->setMonitorClassInNode(NULL);
      tstartNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionEntrySymbolRef(comp()->getMethodSymbol()));
      TR::TreeTop *tstarttt = TR::TreeTop::create(comp(),tstartNode,NULL,NULL);


      tstartblock->getEntry()->join(tstarttt);
      tstarttt->join(tstartblock->getExit());

//      traceMsg(comp(), "\nTM: \n\tadded edge between fhBlock %d(%p) and monenter block_%d(%p)",fhBlock->getNumber(),fhBlock,monitorblock->getNumber(),monitorblock);

      //CFG Modifications -- recall that removeEdge hides complex code under its seemingly simple name, and must be done last

      cfg->addEdge(tophalfmonitorblock,tstartblock);
      cfg->addEdge(tstartblock,critSectStart);
      cfg->removeEdge(tophalfmonitorblock,monitorblock);

      //just a silly test to verify we've not created a loop in the IL
      TR::TreeTop *tmp = comp()->getMethodSymbol()->getLastTreeTop();
      // Perhaps this should be modularized, perhaps common it with tm intrinsics?

      bool clonedExit=false;
      TR::Block *tEndBlock = 0 , *monexitblock = 0, *remainderBlock =0;
      ListIterator<TR::TreeTop> exitTrees(&monitor->getExitTrees());
      for (TR::TreeTop *exitTree = exitTrees.getFirst(); exitTree; exitTree = exitTrees.getNext())
         {
         debugTrace(tracer(),"\nTM: about to transform exit tree %p with node %p \n",exitTree,exitTree->getNode());


         if(exitTree->getNode()->getOpCodeValue() != TR::monexit && (exitTree->getNode()->getFirstChild() && exitTree->getNode()->getFirstChild()->getOpCodeValue() != TR::monexit)  )
            {
            debugTrace(tracer(),"\nTM: Already transformed exit tree %p with node %p. Not doing it again.",exitTree,exitTree->getNode());
            continue;
            }

         TR::Node *exitChildNode = 0;
         if(exitTree->getNode()->getOpCodeValue() == TR::monexit)
            exitChildNode = exitTree->getNode()->getFirstChild();
         else if(exitTree->getNode()->getFirstChild() && exitTree->getNode()->getFirstChild()->getOpCodeValue() == TR::monexit)
            exitChildNode = exitTree->getNode()->getFirstChild()->getFirstChild();
         else
            TR_ASSERT(0, "Error, could not find monexit tree\n");

         TR::TreeTop *tmpExitTree = TR::TreeTop::create(comp(),TR::Node::create(TR::treetop, 1,exitChildNode));

         exitTree->insertBefore(tmpExitTree);

         tEndBlock = exitTree->getEnclosingBlock()->split(exitTree,cfg,true,true);
         TR::Block *tophalfexitblock = tmpExitTree->getEnclosingBlock();

         if(exitTree->getNextTreeTop()->getNode()->getOpCodeValue() == TR::BBEnd)
            remainderBlock = exitTree->getEnclosingBlock()->getExit()->getNextTreeTop()->getEnclosingBlock();
         else
            remainderBlock =  exitTree->getEnclosingBlock()->split(exitTree->getNextTreeTop(),cfg,true,true);


         //append the branch that checks temporary t
         TR::Node *exitif = TR::Node::createif(TR::ificmple,TR::Node::createWithSymRef(exitTree->getNode(),TR::iload,0,tempSymRef),TR::Node::create(exitTree->getNode(),TR::iconst,0,0),NULL);

         tmpExitTree->getEnclosingBlock()->getExit()->insertBefore(TR::TreeTop::create(comp(),exitif,NULL,NULL));

         if(!clonedExit)
            {
            TR_BlockCloner exitCloner(cfg,true,true); //this will now clone the exit successors
            monexitblock = exitCloner.cloneBlocks(tEndBlock,tEndBlock);

            comp()->getMethodSymbol()->getLastTreeTop()->join(monexitblock->getEntry());

            monexitblock->append(TR::TreeTop::create(comp(),TR::Node::create(exitTree->getNode(),TR::Goto,0,remainderBlock->getEntry())));

            //cfg->addEdge(monexitblock,remainderBlock);

            debugTrace(tracer(),"\nTM: cloned block_%d(%p) to be used as monexit block.monexitblock = %d(%p)",tEndBlock->getNumber(),tEndBlock,monexitblock->getNumber(),monexitblock);
            // There is a problem with sharing the same monexit when you have an exception block that unlocks the monitor as well.
            // I suspect the problem is that the original monexit had an exception successor which could unlock the monitor.
            // this results in some funky cfg afterwards where the monexit block has the exception block as a predecessor and an exception successor
           // clonedExit = true;
            }
          exitif->setBranchDestination(monexitblock->getEntry());
          cfg->addEdge(tophalfexitblock,monexitblock);

          /*
          TR::Node::recreate(exitTree->getNode(), TR::tfinish);
          exitTree->getNode()->setNumChildren(0);
          exitTree->getNode()->setAndIncChild(0,TR::Node::create(exitTree->getNode(),TR::iconst,0,0));
          exitTree->getNode()->setBranchDestination(monexitblock->getEntry());
          */
          TR::Node * tfinishNode = TR::Node::create(exitTree->getNode(),TR::tfinish,0,0);
          tfinishNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionExitSymbolRef(comp()->getMethodSymbol()));
          TR::TreeTop *tfinishtt = TR::TreeTop::create(comp(),tfinishNode,NULL,NULL);



         // tEndBlock->getEntry()->join(tfinishtt);
          //tfinishtt->join(exitTree);      //tEndBlock->getExit());
          debugTrace(tracer(),"About to remove dead monexit tree %p node %p (replaced by tfinish at %p %p\n",exitTree,exitTree->getNode(),tfinishtt,tfinishtt->getNode());
          //exitTree->insertBefore(tfinishtt);
          exitTree->insertAfter(tfinishtt);
          if(exitTree->getNode()->getOpCodeValue() == TR::monexit)
            TR::TreeTop::removeDeadTrees(comp(),exitTree,exitTree->getNextTreeTop());
          else
            {
            int16_t numChildren = exitTree->getNode()->getNumChildren();
            for (int16_t child = 0; child<numChildren; ++child)
               {
               TR::Node * node = exitTree->getNode()->getChild(child);
               if(node->getOpCodeValue() == TR::monexit)
                  {
                  TR::Node::recreate(node, TR::PassThrough);
                  debugTrace(tracer(),"Replacing monexit node %p with a pass through under tt %p node %p\n",node,exitTree,exitTree->getNode());
                  break;
                  }
               }
            }
          //exitTree->getNode()->recursivelyDecReferenceCount();
          //cfg->addEdge(tEndBlock,monexitblock);

         }

      // Insert tabort for all OSR induce blocks
      ListIterator<TR::TreeTop> osrInduceBlocks(&monitor->getOSRGuards());
      for (TR::TreeTop *osrInduceTT = osrInduceBlocks.getFirst(); osrInduceTT; osrInduceTT = osrInduceBlocks.getNext())
         {
         debugTrace(tracer(),"\nTM: prepending TABORT to OSR induce block [%p] for monent [%p]\n", monitor->getMonitorNode(), osrInduceTT->getNode());
         TR::Node *tabortNode = TR::Node::create(osrInduceTT->getNode(), TR::tabort, 0, 0);
         tabortNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionAbortSymbolRef(comp()->getMethodSymbol()));
         TR::TreeTop *tabortTT = TR::TreeTop::create(self()->comp(), tabortNode, osrInduceTT);
         }
      }

   }

#endif

bool TR::MonitorElimination::killsReadMonitorProperty(TR::Node *node)
   {
   if ((node->getOpCodeValue() == TR::monexit) ||
       (node->getOpCodeValue() == TR::monent) ||
       (node->exceptionsRaised() != 0) ||
       node->getOpCode().isStoreIndirect() ||
       (node->getOpCode().isStore() &&
        node->getSymbolReference()->getSymbol()->isStatic()) ||
       (node->getOpCodeValue() == TR::asynccheck))
      return true;

   return false;
   }



bool TR::MonitorElimination::transformIntoReadMonitor()
   {
   TR::TreeTop *treeTop = comp()->getStartTree();
   TR::Block *block = NULL;
   TR::TreeTop *lastMonitor = NULL;
   TR::Node *lastMonitorNode = NULL;
   TR::Block *lastMonitorBlock = NULL;
   int32_t lastMonitorTreeNum = -1;
   int32_t treeNum = -1;
   int32_t startTreeNum = 0;
   for (; treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         TR::Block *prevBlock = block;
         block = node->getBlock();
         bool killMonitors = true;
         if (prevBlock &&
             (prevBlock->getSuccessors().size() == 1) &&
             (block->getPredecessors().size() == 1) &&
             (prevBlock->getSuccessors().front()->getTo() == block))
            killMonitors = false;

         if (killMonitors)
            {
            lastMonitor = NULL;
            lastMonitorNode = NULL;
            lastMonitorBlock = NULL;
            treeNum = -1;
            }

         startTreeNum = treeNum;
         }

      treeNum++;

      bool isNullCheck = false;
      if (node->getOpCodeValue() == TR::NULLCHK)
         {
         isNullCheck = true;
         node = node->getFirstChild();
         }

      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (killsReadMonitorProperty(node) || isNullCheck)
         {
         lastMonitor = NULL;
         lastMonitorNode = NULL;
         lastMonitorBlock = NULL;
         }

      if (node->getOpCodeValue() == TR::monent)
         {
         lastMonitor = treeTop;
         lastMonitorNode = node;
         lastMonitorBlock = block;
         lastMonitorTreeNum = ((treeNum - startTreeNum)-1);

         if ((node->isReadMonitor()) ||
             (!node->getFirstChild()->getOpCode().hasSymbolReference() ||
              node->getFirstChild()->getSymbolReference()->isUnresolved()))
            {
            lastMonitor = NULL;
            lastMonitorNode = NULL;
            lastMonitorBlock = NULL;
            }
         }

      if (node->getOpCodeValue() == TR::BBEnd)
         {
         if (lastMonitorNode)
            {
            recognizeIfThenReadRegion(lastMonitor, lastMonitorNode, lastMonitorTreeNum, lastMonitorBlock, block);
            }
         }
      }

   return true;
   }



void TR::MonitorElimination::recognizeIfThenReadRegion(TR::TreeTop *lastMonitor, TR::Node *lastMonitorNode, int32_t lastMonitorTreeNum, TR::Block *lastMonitorBlock, TR::Block *block)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::CFGEdgeList& edges = block->getSuccessors();
   TR_ScratchList<TR::Block> intermediateBlocks(trMemory());
   TR_ScratchList<TR::Block> blocksNeedingAdjustment(trMemory());
   if (edges.size() == 2)
      {
      TR::Block * block1 = toBlock(edges.front()->getTo());
      TR::Block * block2 = toBlock((*(++edges.begin()))->getTo());

      TR::Block *mergeBlock1 = NULL, *mergeBlock2 = NULL;
      if (block1->getSuccessors().size() == 1)
         mergeBlock1 = toBlock(block1->getSuccessors().front()->getTo());

      if (mergeBlock1 &&
          (((block2->getSuccessors().size() == 1) &&
            block2->hasSuccessor(mergeBlock1)) ||
           (mergeBlock1 == block2)))
         {
         mergeBlock2 = mergeBlock1;
         intermediateBlocks.add(block1);
         if (mergeBlock1 != block2)
            intermediateBlocks.add(block2);
         }
      else
         {
         if (block2->getSuccessors().size() == 1)
            mergeBlock2 = toBlock(block2->getSuccessors().front()->getTo());

         if (mergeBlock2 &&
             (((block1->getSuccessors().size() == 1) &&
               block1->hasSuccessor(mergeBlock2)) ||
              (mergeBlock2 == block1)))
            {
            mergeBlock1 = mergeBlock2;
            intermediateBlocks.add(block2);
            if (mergeBlock2 != block1)
               intermediateBlocks.add(block1);
            }
         }

      if (mergeBlock1 &&
          mergeBlock1 == mergeBlock2 &&
          mergeBlock1 != cfg->getEnd())
         {
         TR::Node *monexitNode = NULL;
         if (preservesReadRegion(lastMonitorNode, mergeBlock1, &monexitNode) &&
             performTransformation(comp(), "%s Found a locked region that was almost read-only in %s and num intermediate blocks = %d\n", OPT_DETAILS, comp()->signature(), intermediateBlocks.getSize()))
            {
            _invalidateUseDefInfo = true;
            _invalidateValueNumberInfo = true;
            _invalidateAliasSets = true;

            //printf("Found a locked region that was almost read-only in %s and num intermediate blocks = %d\n", comp()->signature(), intermediateBlocks.getSize());
            //
            // Intermediate blocks have a kill point that prevented this from
            // being marked as a read region.
            //
            bool canBeAdjusted = intermediateBlocks.isSingleton() ? true : false;
            ListIterator<TR::Block> interIt(&intermediateBlocks);
            for (TR::Block *nextBlock = interIt.getFirst(); nextBlock; nextBlock = interIt.getNext())
               {
               if (!preservesReadRegion(lastMonitorNode, nextBlock, &monexitNode))
                  {
                  blocksNeedingAdjustment.add(nextBlock);
                  if (block->getNextBlock() != nextBlock)
                     {
                     canBeAdjusted = false;
                     break;
                     }
                  }
               else
                  canBeAdjusted = true;
               }

            if (canBeAdjusted)
               {
               if (block->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock()->getFrequency() <= 0)
                  canBeAdjusted = false;

               if (canBeAdjusted)
                  {
                  ListIterator<TR::Block> interIt(&intermediateBlocks);
                  for (TR::Block *nextBlock = interIt.getFirst(); nextBlock; nextBlock = interIt.getNext())
                     {
                     if (nextBlock->getFrequency() > 0)
                        {
                        canBeAdjusted = false;
                        break;
                        }
                     }
                  }
               }

            if (canBeAdjusted &&
                lastMonitorNode &&
                monexitNode)
               {
                 //lastMonitorNode->setReadMonitor(true, comp());
                 //monexitNode->setReadMonitor(true, comp());
               TR::CFG *cfg = comp()->getFlowGraph();
               ListIterator<TR::Block> interIt(&blocksNeedingAdjustment);
               for (TR::Block *nextBlock = interIt.getFirst(); nextBlock; nextBlock = interIt.getNext())
                  {
                  TR::Block *blockWithKill = nextBlock;
                  TR::Block *clonedBlockWithIf = adjustBlockToCreateReadMonitor(lastMonitor, lastMonitorNode, lastMonitorTreeNum, lastMonitorBlock, nextBlock, block);
                  //dumpOptDetails(comp(), "lastTree %p %s blockNum %d\n", clonedBlockWithIf->getLastRealTreeTop()->getNode(), clonedBlockWithIf->getLastRealTreeTop()->getNode()->getOpCode().getName(), clonedBlockWithIf->getNumber());
                  TR::Block *targetBlock = clonedBlockWithIf->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock();

                  TR::Block *clonedTargetBlock = NULL;
                  TR::Block *gotoForFallThrough = NULL;
                  TR_BlockCloner cloner(comp()->getFlowGraph(), true, false);
                  TR::TreeTop *lastTree = comp()->getMethodSymbol()->getLastTreeTop();
                  //dumpOptDetails(comp(), "lastTree %p\n", lastTree->getNode());
                  if (targetBlock != mergeBlock1)
                     {
                     clonedTargetBlock = cloner.cloneBlocks(targetBlock, targetBlock);
                     lastTree->join(clonedTargetBlock->getEntry());
                     clonedTargetBlock->getExit()->setNextTreeTop(NULL);
                     lastTree = clonedTargetBlock->getExit();

                     TR::Block *nextBlock = targetBlock->getNextBlock();
                     if (nextBlock &&
                         (nextBlock != mergeBlock1) &&
                         targetBlock->hasSuccessor(nextBlock))
                        {
                        TR::CFGEdge *e = targetBlock->getEdge(nextBlock);
                        TR::Block *gotoBlock = TR::Block::createEmptyBlock(targetBlock->getEntry()->getNode(), comp(), e->getFrequency(), targetBlock);
                        TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(targetBlock->getEntry()->getNode(), TR::Goto, 0, nextBlock->getEntry()));
                        gotoBlock->append(gotoTreeTop);
                        lastTree->join(gotoBlock->getEntry());
                        gotoBlock->getExit()->setNextTreeTop(NULL);
                        clonedTargetBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), nextBlock->getEntry(), gotoBlock->getEntry());
                        lastTree = gotoBlock->getExit();
                        TR::CFGEdge *newEdge = NULL;
                        cfg->addNode(gotoBlock);
                        newEdge = TR::CFGEdge::createEdge(clonedTargetBlock,  gotoBlock, trMemory());
                        cfg->addEdge(newEdge);
                        newEdge = TR::CFGEdge::createEdge(gotoBlock,  nextBlock, trMemory());
                        cfg->addEdge(newEdge);
                        gotoForFallThrough = gotoBlock;
                        }

                     for (auto nextEdge = targetBlock->getExceptionSuccessors().begin(); nextEdge != targetBlock->getExceptionSuccessors().end(); ++nextEdge)
                        {
                        TR::Block *succ = (*nextEdge)->getTo()->asBlock();
                        cfg->addExceptionEdge(clonedTargetBlock, succ);
                        if (gotoForFallThrough)
                           cfg->addExceptionEdge(gotoForFallThrough, succ);
                        }
                     for (auto nextEdge = targetBlock->getSuccessors().begin(); nextEdge != targetBlock->getSuccessors().end(); ++nextEdge)
                        {
                        TR::Block *succ = (*nextEdge)->getTo()->asBlock();
                        if (gotoForFallThrough &&
                            (succ == nextBlock))
                           {
                           // Already added above
                           //
                           }
                        else
                           cfg->addEdge(clonedTargetBlock, succ);
                        }
                     }

                  TR::Block *clonedMergeBlock1 = cloner.cloneBlocks(mergeBlock1, mergeBlock1);
                  lastTree->join(clonedMergeBlock1->getEntry());
                  //dumpOptDetails(comp(), "lastTree %p entry %p blockNum %d\n", lastTree->getNode(), clonedMergeBlock1->getEntry()->getNode(), clonedMergeBlock1->getNumber());
                  clonedMergeBlock1->getExit()->setNextTreeTop(NULL);
                  lastTree = clonedMergeBlock1->getExit();

                  nextBlock = mergeBlock1->getNextBlock();
                  TR::Block *gotoBlock = NULL;
                  if (nextBlock &&
                      mergeBlock1->hasSuccessor(nextBlock))
                     {
                     TR::CFGEdge *e = mergeBlock1->getEdge(nextBlock);
                     gotoBlock = TR::Block::createEmptyBlock(mergeBlock1->getEntry()->getNode(), comp(), e->getFrequency(), mergeBlock1);
                     TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(mergeBlock1->getEntry()->getNode(), TR::Goto, 0, nextBlock->getEntry()));
                     gotoBlock->append(gotoTreeTop);
                     lastTree->join(gotoBlock->getEntry());
                     gotoBlock->getExit()->setNextTreeTop(NULL);
                     clonedMergeBlock1->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), nextBlock->getEntry(), gotoBlock->getEntry());
                     lastTree = gotoBlock->getExit();
                     TR::CFGEdge *newEdge = NULL;
                     cfg->addNode(gotoBlock);
                     newEdge = TR::CFGEdge::createEdge(clonedMergeBlock1,  gotoBlock, trMemory());
                     cfg->addEdge(newEdge);
                     newEdge = TR::CFGEdge::createEdge(gotoBlock,  nextBlock, trMemory());
                     cfg->addEdge(newEdge);
                     }

                  TR::TreeTop *nextBlockLastTree = blockWithKill->getLastRealTreeTop();
                  if (nextBlockLastTree->getNode()->getOpCodeValue() == TR::Goto)
                     nextBlockLastTree = nextBlockLastTree->getPrevTreeTop();

                  TR::Node *dupNode = monexitNode->duplicateTree();
                  dupNode->setReadMonitor(false);
                  if (dupNode->getOpCodeValue() == TR::monexit)
                     dupNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, dupNode, comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(dupNode->getSymbolReference()->getOwningMethodSymbol(comp())));
                  TR::TreeTop *monTree = TR::TreeTop::create(comp(), dupNode, NULL, NULL);
                  TR::TreeTop *succTree = nextBlockLastTree->getNextTreeTop();
                  nextBlockLastTree->join(monTree);
                  monTree->join(succTree);
                  nextBlockLastTree = monTree;
                  dupNode = lastMonitorNode->duplicateTree();
                  dupNode->setReadMonitor(true);
                  if (dupNode->getOpCodeValue() == TR::monent)
                     dupNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, dupNode, comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(dupNode->getSymbolReference()->getOwningMethodSymbol(comp())));
                  monTree = TR::TreeTop::create(comp(), dupNode, NULL, NULL);
                  nextBlockLastTree->join(monTree);
                  monTree->join(succTree);

                  for (auto nextEdge = mergeBlock1->getExceptionSuccessors().begin(); nextEdge != mergeBlock1->getExceptionSuccessors().end(); ++nextEdge)
                     {
                     TR::Block *succ = (*nextEdge)->getTo()->asBlock();
                     cfg->addExceptionEdge(clonedMergeBlock1, succ);
                     if (gotoBlock)
                        cfg->addExceptionEdge(gotoBlock, succ);
                     }
                  for (auto nextEdge = mergeBlock1->getSuccessors().begin(); nextEdge != mergeBlock1->getSuccessors().end(); ++nextEdge)
                     {
                     TR::Block *succ = (*nextEdge)->getTo()->asBlock();
                     if (gotoBlock &&
                         (succ == nextBlock))
                       {
                       // Already added above
                       //
                       }
                     else
                       cfg->addEdge(clonedMergeBlock1, succ);
                     }

                  //if (!nextBlock ||
                  //    !clonedMergeBlock1->hasSuccessor(nextBlock))
                  //   cfg->removeEdge(clonedMergeBlock1, nextBlock);

                  if (clonedTargetBlock)
                     {
                     cfg->addEdge(clonedBlockWithIf, clonedTargetBlock);
                     cfg->removeEdge(clonedBlockWithIf, targetBlock);
                     clonedBlockWithIf->getLastRealTreeTop()->getNode()->setBranchDestination(clonedTargetBlock->getEntry());
                     if (!gotoForFallThrough)
                        {
                        cfg->addEdge(clonedTargetBlock, clonedMergeBlock1);
                        cfg->removeEdge(clonedTargetBlock, mergeBlock1);
                        }
                     }
                  else
                     {
                     cfg->addEdge(clonedBlockWithIf, clonedMergeBlock1);
                     cfg->removeEdge(clonedBlockWithIf, mergeBlock1);
                     clonedBlockWithIf->getLastRealTreeTop()->getNode()->setBranchDestination(clonedMergeBlock1->getEntry());
                     }
                  }
               lastMonitorNode->setReadMonitor(true);
               monexitNode->setReadMonitor(true);
               }
            }
         }
      }
   }




TR::Block *TR::MonitorElimination::adjustBlockToCreateReadMonitor(TR::TreeTop *lastMonitor, TR::Node *lastMonitorNode, int32_t lastMonitorTreeNum, TR::Block *lastMonitorBlock, TR::Block *blockToBeAdjusted, TR::Block *blockWithIf)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   cfg->setStructure(NULL);
   TR_BlockCloner cloner(comp()->getFlowGraph(), true, false);
   TR::Block *newMonitorBlock = cloner.cloneBlocks(lastMonitorBlock, blockWithIf);
   TR::TreeTop *cursorTree = newMonitorBlock->getEntry();
   TR::TreeTop *prevCursor = NULL;

   TR::Block *cursorBlock = lastMonitorBlock;
   TR::Block *clonedBlockWithIf = NULL;
   int32_t treeNum = -1;
   while (cursorTree)
      {
      treeNum++;
      TR::TreeTop *next = cursorTree->getNextTreeTop();
      TR::Node *cursorNode = cursorTree->getNode();
      if (cursorNode->getOpCodeValue() == TR::BBStart)
         {
         prevCursor = cursorTree;
         cursorTree = next;
         if (cursorBlock == blockWithIf)
            clonedBlockWithIf = cursorNode->getBlock();
         cursorBlock = cursorBlock->getNextBlock();
         continue;
         }

      if (treeNum < lastMonitorTreeNum)
          TR::TransformUtil::removeTree(comp(), cursorTree);
      else if (treeNum == lastMonitorTreeNum)
         {
         TR::Node *dupNode = cursorNode->duplicateTree();
         TR::Node *monNode = dupNode;
         while (monNode->getOpCodeValue() != TR::monent)
            monNode = monNode->getFirstChild();

         TR::Node::recreate(monNode, TR::monexit);
         monNode->setReadMonitor(true);
         TR::TreeTop *monTree = TR::TreeTop::create(comp(), dupNode, NULL, NULL);
         TR::TreeTop *prevTree = cursorTree->getPrevTreeTop();
         prevTree->join(monTree);
         monTree->join(cursorTree);
         //break;
         }

      prevCursor = cursorTree;
      cursorTree = next;
      //dumpOptDetails(comp(), "cursorNode %p %s treeNum %d lastTreeNum %d\n", cursorNode, cursorNode->getOpCode().getName(), treeNum, lastMonitorTreeNum);

      if (cursorNode->getOpCodeValue() == TR::BBEnd)
         {
         if (clonedBlockWithIf)
            break;
         }
      }

   cfg->addEdge(clonedBlockWithIf, blockWithIf->getNextBlock());
   cfg->addEdge(clonedBlockWithIf, blockWithIf->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock());
   cfg->addEdge(blockWithIf, newMonitorBlock);
   cfg->removeEdge(blockWithIf, blockWithIf->getNextBlock());

   TR::TreeTop *insertionPoint = blockWithIf->getExit();
   TR::TreeTop *nextTree = insertionPoint->getNextTreeTop();
   insertionPoint->join(newMonitorBlock->getEntry());
   prevCursor->join(nextTree);
   return clonedBlockWithIf;
   }


bool TR::MonitorElimination::preservesReadRegion(TR::Node *lastMonitorNode, TR::Block *block, TR::Node **monexitNode)
   {
   TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
   int32_t lastNodeValueNumber = -1;
   if (info)
      lastNodeValueNumber = info->getValueNumber(lastMonitorNode->getFirstChild());

   TR::TreeTop *treeTop = block->getEntry();
   TR::TreeTop *exitTree = block->getExit();
   for (; treeTop != exitTree; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         block = node->getBlock();

      bool isNullCheck = false;
      if (node->getOpCodeValue() == TR::NULLCHK)
         {
         node = node->getFirstChild();
         isNullCheck = true;
         }

      if (node->getOpCodeValue() == TR::treetop)
         {
         node = node->getFirstChild();
         }

      if (node->getOpCodeValue() == TR::monexit)
         {
         int32_t valueNumber = -1;
         if (info)
            valueNumber = info->getValueNumber(node->getFirstChild());
         if ((valueNumber < 0) ||
             (valueNumber != lastNodeValueNumber))
           return false;

        if (!node->getFirstChild()->getOpCode().hasSymbolReference() ||
            node->getFirstChild()->getSymbolReference()->isUnresolved())
           return false;

         if (!*monexitNode)
            *monexitNode = node;
         else
            return false;

         return true;
         }

      if (isNullCheck ||
          killsReadMonitorProperty(node))
         return false;
      }

   return false;
   }


void TR::MonitorElimination::resetReadMonitors(int32_t index)
   {
   for (int32_t i = index; i > 0; --i)
      {
      TR_ActiveMonitor *containingMonitor = _monitorStack->element(i);
      containingMonitor->setReadMonitor(false);
      //printf("NOT a read monitor %p cause monexit %p in %s\n", containingMonitor->getMonitorNode(), node, comp()->signature());
      //fflush(stdout);
      }
   }

bool TR::MonitorElimination::tagReadMonitors()
   {
   bool foundReadMonitor = false;
   ListIterator<TR_ActiveMonitor> it(&_monitors);
   for (TR_ActiveMonitor *monitor = it.getFirst(); monitor; monitor = it.getNext())
      {
      bool readOnly = monitor->isReadMonitor();

      TR::TreeTop * exitTree;
      ListIterator<TR::TreeTop> exitTrees(&monitor->getExitTrees());
      if (readOnly)
         {
           //printf("Tagging a read monitor in %s\n", comp()->signature());
           //fflush(stdout);
         foundReadMonitor = true;
         if (monitor->getMonitorNode()->getOpCodeValue() == TR::monent)
            {
            monitor->getMonitorNode()->setReadMonitor(true);
            //dumpOptDetails(comp(), "Tagging a read monitor enter %p\n", monitor->getMonitorNode());
            }

         for (exitTree = exitTrees.getFirst(); exitTree; exitTree = exitTrees.getNext())
            {
            TR::Node *monexitNode = exitTree->getNode();
            if (exitTree->getNode()->getOpCodeValue() == TR::treetop ||
                exitTree->getNode()->getOpCodeValue() == TR::NULLCHK)
               monexitNode = monexitNode->getFirstChild();
            //dumpOptDetails(comp(), "Tagging a read monitor exit %p\n", monexitNode);
            if (monexitNode->getOpCodeValue() == TR::monexit)
               monexitNode->setReadMonitor(true);
            }
         }
      }
   return foundReadMonitor;
   }



void TR::MonitorElimination::removeMonitorNode(TR::Node *node)
   {
   TR::Node *child = node->getFirstChild();
   if (node->getOpCodeValue() == TR::NULLCHK)
      {
      if (performTransformation(comp(), "%s Replacing monitor node [%p] by passthrough node\n", OPT_DETAILS, child))
         {
#if DO_THE_REMOVAL
         TR::Node::recreate(child, TR::PassThrough);
#endif
         }
      }
   else if (node->getOpCodeValue() == TR::treetop)
      {
      if (performTransformation(comp(), "%s Removing monitor node [%p]\n", OPT_DETAILS, child))
         {
#if DO_THE_REMOVAL
         if (child->getOpCodeValue() == TR::monent || child->getOpCodeValue() == TR::monexit)
            {
            TR::Node *grandChild = child->getFirstChild();
            grandChild->incReferenceCount();
            node->setFirst(grandChild);
            child->recursivelyDecReferenceCount();
            }
#endif
         }
      }
   else
      {
      TR_ASSERT(node->getOpCodeValue() == TR::monent ||
             node->getOpCodeValue() == TR::monexit, "Expected monitor node");
      if (performTransformation(comp(), "%s Replacing monitor node [%p] by treetop node\n", OPT_DETAILS, node))
         {
#if DO_THE_REMOVAL
         TR::Node::recreate(node, TR::treetop);
#endif
         }
      }
   }




bool TR::MonitorElimination::markBlocksAtSameNestingLevel(TR_Structure *structure, TR_BitVector *blocksAtCurrentNestingLevel)
   {
   if (structure->asBlock())
      blocksAtCurrentNestingLevel->set(structure->getNumber());
   else
      {
      TR_RegionStructure *region = structure->asRegion();
      bool isLoop = region->isNaturalLoop();

      if (!isLoop &&
          !region->isAcyclic())
         return true;

      TR_BitVector *blocksInRegion = blocksAtCurrentNestingLevel;
      if (isLoop)
         {
         blocksInRegion = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, notGrowable);
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
         if (markBlocksAtSameNestingLevel(subStruct, blocksInRegion))
            return true;
         }

      if (isLoop ||
          comp()->getFlowGraph()->getStructure() == region)
        region->setBlocksAtSameNestingLevel(blocksInRegion);
      }

   return false;
   }



void TR::MonitorElimination::collectCFGBackEdges(TR_StructureSubGraphNode *loopEntry)
   {
   for (auto edge = loopEntry->getPredecessors().begin(); edge != loopEntry->getPredecessors().end(); ++edge)
      {
      TR_Structure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure();
      pred->collectCFGEdgesTo(loopEntry->getNumber(), &_cfgBackEdges);
      }
   }



// Temporarily made non-recursive because of stack overflow;
// should be able to handle this in a better non-recursive manner
//
void TR::MonitorElimination::collectPredsAndSuccs(TR::CFGNode *startNode, TR_BitVector *visitedNodes, TR_BitVector **predecessorInfo, TR_BitVector **successorInfo, List<TR::CFGEdge> *cfgBackEdges, TR_BitVector *loopEntryBlocks, TR::Compilation *comp)
   {
   TR_ScratchList<TR_RecursionState> analysisQueue(comp->trMemory());
   analysisQueue.add(new (comp->trStackMemory()) TR_RecursionState(startNode, startNode, 2));
   bool isFirst = true;
   TR_BitVector *subtraction = new (comp->trStackMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory(), stackAlloc, notGrowable);

   while (!analysisQueue.isEmpty())
      {
      bool isStart = isFirst;
      isFirst = false;

      TR_RecursionState *recState = analysisQueue.getListHead()->getData();
      if (recState->getState() < 2)
         analysisQueue.popHead();

      TR::CFGNode *node = recState->getBlock();

      bool somethingChanged = false;
      //if (node == comp()->getFlowGraph()->getStart())
      if (isStart)
         somethingChanged = true;

      TR_PredecessorIterator pit(node);
      TR::CFGEdge *edge = pit.getFirst();
      bool continueFlag = false;

      if ((recState->getState() == 0) ||
          (recState->getState() == 2))
         {
         if (recState->getState() == 0)
            {
            for (; edge; edge = pit.getNext())
               {
               TR::CFGNode *pred = edge->getFrom();
               if (pred == recState->getPred())
                  {
                  edge = pit.getNext();
                  break;
                  }
               }
            }

         for (; edge; edge = pit.getNext())
            {
            TR::CFGNode *pred = edge->getFrom();
            if (!visitedNodes->get(pred->getNumber()) &&
                !(loopEntryBlocks->get(node->getNumber()) &&
                  cfgBackEdges->find(edge)))
               {
               analysisQueue.add(new (comp->trStackMemory()) TR_RecursionState(pred, node, 0));
               analysisQueue.add(new (comp->trStackMemory()) TR_RecursionState(pred, pred, 2));
               continueFlag = true;
               break;
               //collectPredsAndSuccs(pred, visitedNodes);
               }
            else
               {
               if (!(loopEntryBlocks->get(node->getNumber()) &&
                     cfgBackEdges->find(edge)))
                  {
                  *subtraction = *predecessorInfo[pred->getNumber()];
                  *subtraction -= *predecessorInfo[node->getNumber()];
                  if (!subtraction->isEmpty())
                     {
                     somethingChanged = true;
                     *predecessorInfo[node->getNumber()] |= *predecessorInfo[pred->getNumber()];
                     }

                  if (!predecessorInfo[node->getNumber()]->get(pred->getNumber()))
                     {
                     somethingChanged = true;
                     predecessorInfo[node->getNumber()]->set(pred->getNumber());
                     }

                  if (!loopEntryBlocks->get(node->getNumber()) &&
                      predecessorInfo[node->getNumber()]->get(node->getNumber()))
                     {
                     traceMsg(comp, "Node is %d\n", node->getNumber());
                     TR_ASSERT(0, "CFG node cannot have itself as a predecessor in a CFG walk ignoring back edges\n");
                     }
                  }
               else
                  {
                  if (!predecessorInfo[node->getNumber()]->get(node->getNumber()))
                     {
                     somethingChanged = true;
                     predecessorInfo[node->getNumber()]->set(node->getNumber());
                     }
                  }
               }
            }

         if (continueFlag)
            continue;

         visitedNodes->set(node->getNumber());
         }

      if (somethingChanged ||
          (recState->getState() == 1) /* ||
          (recState->getState() == 2) */)
         {
         bool continueFlag2 = false;
         TR_SuccessorIterator sit(node);
         edge = sit.getFirst();

         if (recState->getState() == 1)
            {
            for (; edge; edge = sit.getNext())
               {
               TR::CFGNode *succ = edge->getTo();
               if (succ == recState->getSucc())
                  {
                  successorInfo[node->getNumber()]->set(succ->getNumber());
                  *successorInfo[node->getNumber()] |= *successorInfo[succ->getNumber()];
                  edge = sit.getNext();
                  break;
                  }
               }
            }

         for (;edge; edge = sit.getNext())
            {
            TR::CFGNode *succ = edge->getTo();
            if (!(loopEntryBlocks->get(succ->getNumber()) &&
                  cfgBackEdges->find(edge)))
               {
               if (succ != comp->getFlowGraph()->getEnd())
                  {
                  analysisQueue.add(new (comp->trStackMemory()) TR_RecursionState(succ, node, 1));
                  analysisQueue.add(new (comp->trStackMemory()) TR_RecursionState(succ, succ, 2));
                  continueFlag2 = true;
                  break;
                  }
               else
                  {
                  successorInfo[node->getNumber()]->set(succ->getNumber());
                  predecessorInfo[succ->getNumber()]->set(node->getNumber());
                  *predecessorInfo[succ->getNumber()] |= *predecessorInfo[node->getNumber()];
                  }
               }
            }

         if (continueFlag2)
            continue;
         }

      if (recState->getState() == 2)
         analysisQueue.popHead();
      }
   }



void TR::MonitorElimination::coarsenMonitorRanges()
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   int32_t numBlocks = cfg->getNextNodeNumber();

   if (numBlocks <= 0)
     return;

   if (!cfg->getStructure())
     return;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _containsAsyncCheck  = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _containsCalls = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _containsExceptions = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _containsMonents = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _containsMonexits = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _guardedVirtualCallBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _loopEntryBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _lockedObjects = new (trStackMemory()) TR_BitVector(1, trMemory(), stackAlloc, growable);
   _multiplyLockedObjects = new (trStackMemory()) TR_BitVector(1, trMemory(), stackAlloc, growable);

   _symbolsWritten = (TR_BitVector **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR_BitVector *));
   _monentTrees = (TR::TreeTop **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR::TreeTop *));
   _monexitTrees = (TR::TreeTop **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR::TreeTop *));
   _monentBlockInfo = (int32_t *) trMemory()->allocateStackMemory(numBlocks* sizeof(int32_t));
   _monexitBlockInfo = (int32_t *) trMemory()->allocateStackMemory(numBlocks* sizeof(int32_t));

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();
   int32_t i;
   for (i=0;i<numBlocks;i++)
      {
      _monentBlockInfo[i] = -2;
      _monexitBlockInfo[i] = -2;
      _symbolsWritten[i] = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);
      }

   _symRefsInSimpleLockedRegion = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);
   _storedSymRefsInSimpleLockedRegion = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);

   TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
   int32_t numberOfNodes = info->getNumberOfNodes();
   _safeValueNumbers = (int32_t *) trMemory()->allocateStackMemory(numberOfNodes* sizeof(int32_t));

   for (i=0;i<numberOfNodes;i++)
     _safeValueNumbers[i] = -1;

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   TR_UseDefInfo::BitVector defs1(comp()->allocator());
   TR_UseDefInfo::BitVector defs2(comp()->allocator());
   defs1.GrowTo(useDefInfo->getTotalNodes());
   defs2.GrowTo(useDefInfo->getTotalNodes());

   _coarsenedMonitors = new (trStackMemory()) TR_BitVector(numberOfNodes, trMemory(), stackAlloc, notGrowable);

   int32_t blockNum = -1;
   bool reanalyzeBlock = false;
   bool containsCall = false;
   TR::TreeTop *treeTop = comp()->getStartTree();
   TR::TreeTop *currentMonexit = NULL, *blockStart = NULL;
   TR::Block *block = NULL;
   vcount_t volatileVisitCount = comp()->incVisitCount();
   for (; treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         blockNum = node->getBlock()->getNumber();
         blockStart = treeTop;
         reanalyzeBlock = false;
         }

      if (node->getOpCodeValue() == TR::BBEnd)
         {
         // If we coarsened locally in a previous pass over the trees, then
         // we re-analyze the block because the info might have changed because of the coarsening
         //
         if (reanalyzeBlock)
            {
            treeTop = blockStart;
            //
            // Reset the info for this block to the state it was before analysis
            //
            _monentBlockInfo[blockNum] = -2;
            _monexitBlockInfo[blockNum] = -2;
            _containsCalls->reset(blockNum);
            _containsExceptions->reset(blockNum);
            _containsMonents->reset(blockNum);
            _containsMonexits->reset(blockNum);
            _guardedVirtualCallBlocks->reset(blockNum);
            _loopEntryBlocks->reset(blockNum);
            currentMonexit = NULL;
            reanalyzeBlock = false;

            // we need a new visit count for the volatiles check
            volatileVisitCount = comp()->incVisitCount();
            continue;
            }
         }

      TR::Node *treetopNode = node;

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCodeValue() == TR::NULLCHK)
        node = node->getFirstChild();

      TR::SymbolReference *symRef = NULL;
      if (node->getOpCode().hasSymbolReference())
         symRef = node->getSymbolReference();
      if (node->getOpCode().isStore() ||
          node->mightHaveVolatileSymbolReference())
         {
         if (symRef->sharesSymbol())
            {
            symRef->getUseDefAliases().getAliasesAndUnionWith(*_symbolsWritten[blockNum]);
            }
         else
            _symbolsWritten[blockNum]->set(symRef->getReferenceNumber());
         }
      else if (node->isGCSafePointWithSymRef() && comp()->getOptions()->realTimeGC())
         {
         symRef->getUseDefAliases().getAliasesAndUnionWith(*_symbolsWritten[blockNum]);
         }
      else if (node->getOpCode().isResolveCheck())
         {
         node->getFirstChild()->getSymbolReference()->getUseDefAliases().getAliasesAndUnionWith(*_symbolsWritten[blockNum]);
         }

      TR::Block *guard = NULL;
      bool virtCall = false;
      if (node->getOpCode().isCall() &&
         node->isTheVirtualCallNodeForAGuardedInlinedCall() &&
         (block->getPredecessors().size() == 1) &&
         (guard = block->findVirtualGuardBlock(cfg)) &&
         (guard == block->getPredecessors().front()->getFrom()) &&
         guard->getLastRealTreeTop()->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
   virtCall = true;

      bool exceptionInThisTree = false;
      if ((treetopNode->exceptionsRaised() ||
           (treetopNode->getOpCodeValue() == TR::asynccheck) ||
           node->getOpCode().isCall()) &&
          (node->getOpCodeValue() != TR::monent &&
           node->getOpCodeValue() != TR::monexit))
         {
         if ((treetopNode->getOpCodeValue() != TR::asynccheck) &&
             (!treetopNode->getOpCode().isCheck() ||
              treetopNode->getOpCode().isResolveCheck() ||
              (treetopNode->getOpCode().isNullCheck() &&
               treetopNode->getFirstChild()->getOpCode().isCall())))
            {
            exceptionInThisTree = true;
            }

         if (!virtCall)
            {
            if (node->getOpCodeValue() == TR::asynccheck)
               _containsAsyncCheck->set(blockNum);
            else
               _containsExceptions->set(blockNum);
            }
         }

      bool performsVolatileAccess=node->performsVolatileAccess(volatileVisitCount);
      if (virtCall)
         _guardedVirtualCallBlocks->set(blockNum);
      else if (performsVolatileAccess || exceptionInThisTree)
         {
           //dumpOptDetails(comp(), "Contains calls set for block_%d because of node %p\n", blockNum, treetopNode);
         _containsCalls->set(blockNum);
         containsCall = true;

         /*
         if (_monentBlockInfo[blockNum] == -2)
            {
            if (trace())
               traceMsg(comp(), "Kill monent in block_%d cause %p\n", blockNum, node);
            _monentBlockInfo[blockNum] = -1;
            }

         if (_monexitBlockInfo[blockNum] > -1)
             {
             if (trace())
                traceMsg(comp(), "Kill monexit in block_%d cause %p\n", blockNum, node);
             _monexitBlockInfo[blockNum] = -1;
             currentMonexit = NULL;
             }
         */
         }

      if (node->getOpCodeValue() == TR::monent)
         {
         _containsMonents->set(blockNum);
         if (!node->isSyncMethodMonitor())
            {
            _containsCalls->set(blockNum);
            containsCall = true;
            }

         int32_t lockedObjectValueNumber = info->getValueNumber(node->getFirstChild());
         if (node->getFirstChild()->getOpCode().hasSymbolReference())
            {
            if (_safeValueNumbers[lockedObjectValueNumber] > 0)
               {
               TR::Node *prevNode = useDefInfo->getNode(_safeValueNumbers[lockedObjectValueNumber]);
               if (prevNode->getSymbolReference()->getReferenceNumber() != node->getFirstChild()->getSymbolReference()->getReferenceNumber())
               //if (_safeValueNumbers[lockedObjectValueNumber] != node->getFirstChild()->getSymbolReference()->getReferenceNumber())
                  _safeValueNumbers[lockedObjectValueNumber] = -2;
               else
                  {
                  defs1.Clear();
                  defs2.Clear();
                  TR_UseDefInfo::BitVector tempDefs(comp()->allocator());
                  useDefInfo->getUseDef(tempDefs, _safeValueNumbers[lockedObjectValueNumber]);
                  if (!tempDefs.IsZero())
                     {
                     TR_UseDefInfo::BitVector tempUses(comp()->allocator());
                     TR_UseDefInfo::BitVector::Cursor cursor(tempDefs);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = useDefInfo->getFirstDefIndex() + (int32_t) cursor;
                        //TODO: Temporary fix to overcome case when defIndex = 0
                        if (defIndex < useDefInfo->getFirstRealDefIndex())
                           continue;

                        TR::Node *defNode = useDefInfo->getNode(defIndex);
                        if (defNode->getOpCode().isLoad())
                           useDefInfo->dereferenceDef(defs1, defIndex, tempUses);
                        else
                           defs1[defIndex] = true;
                        }
                     }

                  TR_UseDefInfo::BitVector tempDefs2(comp()->allocator());
                  useDefInfo->getUseDef(tempDefs2, node->getFirstChild()->getUseDefIndex());
                  if (!tempDefs2.IsZero())
                     {
                     TR_UseDefInfo::BitVector tempUses(comp()->allocator());
                     TR_UseDefInfo::BitVector::Cursor cursor(tempDefs2);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = useDefInfo->getFirstDefIndex() + (int32_t) cursor;
                        //TODO: Temporary fix to overcome case when defIndex = 0
                        if (defIndex < useDefInfo->getFirstRealDefIndex())
                           continue;

                        TR::Node *defNode = useDefInfo->getNode(defIndex);
                        if (defNode->getOpCode().isLoad())
                           useDefInfo->dereferenceDef(defs2, defIndex, tempUses);
                        else
                           defs2[defIndex] = true;
                        }
                     }

                  if (!(defs1 == defs2))
                     _safeValueNumbers[lockedObjectValueNumber] = -2;
                  }
               }
            else if (_safeValueNumbers[lockedObjectValueNumber] == -1)
               _safeValueNumbers[lockedObjectValueNumber] = node->getFirstChild()->getUseDefIndex();
               //_safeValueNumbers[lockedObjectValueNumber] = node->getFirstChild()->getSymbolReference()->getReferenceNumber();
            }

         TR_ASSERT((lockedObjectValueNumber > -1), "Invalid value number for locked object\n");

         if (_monexitBlockInfo[blockNum] > -1)
             {
             if (_monexitBlockInfo[blockNum] == lockedObjectValueNumber)
                {
                if ((!containsCall) && performTransformation(comp(), "%s Success: Coarsening monexit %p locally in block_%d\n", OPT_DETAILS, currentMonexit->getNode(), blockNum))
                   {
                   _invalidateUseDefInfo = true;
                   _invalidateValueNumberInfo = true;

                   reanalyzeBlock = true;
                   //currentMonexit->getNode()->recursivelyDecReferenceCount();
                   if (currentMonexit->getNode()->getOpCodeValue() == TR::monexit)
                      TR::Node::recreate(currentMonexit->getNode(), TR::treetop);
                   else
                      TR::Node::recreate(currentMonexit->getNode()->getFirstChild(), TR::PassThrough);

                   if (trace())
                      {
                      traceMsg(comp(),"Success: coarsen locally\n");
                      //printf("Success: coarsen locally in method %s\n", comp()->signature());
                      }

                   TR::TreeTop *prevTree = currentMonexit->getPrevTreeTop();
                   //TR::TreeTop *nextTree = currentMonexit->getNextTreeTop();
                   //prevTree->join(nextTree);
                   //treeTop->getNode()->recursivelyDecReferenceCount();
                   //prevTree = treeTop->getPrevTreeTop();
                   //nextTree = treeTop->getNextTreeTop();
                   //prevTree->join(nextTree);

                  if (treeTop->getNode()->getOpCodeValue() == TR::monent)
                      TR::Node::recreate(treeTop->getNode(), TR::treetop);
                   else
                      TR::Node::recreate(treeTop->getNode()->getFirstChild(), TR::PassThrough);

                   treeTop = prevTree;
                   TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
                   _coarsenedMonitors->set(lockedObjectValueNumber);

                   //dumpOptDetails(comp(), "Success: Coarsening monexit %p locally in block_%d in %s\n", currentMonexit->getNode(), blockNum, comp()->signature());
                   _monexitBlockInfo[blockNum] = -2;
                   currentMonexit = NULL;
                   }
                }
             else
                {
                if (trace())
                   traceMsg(comp(), "Kill monexit in block_%d cause %p\n", blockNum, node);

                _monexitBlockInfo[blockNum] = -1;
                currentMonexit = NULL;
                }
             }

         if ((_monentBlockInfo[blockNum] == -2) &&
             (_monexitBlockInfo[blockNum] == -2) &&
             !_containsCalls->get(blockNum))
            {
            _monentTrees[blockNum] = treeTop;
            _monentBlockInfo[blockNum] = lockedObjectValueNumber;
            }
         else
            {
            _monentBlockInfo[blockNum] = -1;
            }

         if (_lockedObjects->get(lockedObjectValueNumber))
            _multiplyLockedObjects->set(lockedObjectValueNumber);
         else
            _lockedObjects->set(lockedObjectValueNumber);
         }
      else if (node->getOpCodeValue() == TR::monexit)
         {
         containsCall = false;
         _containsMonexits->set(blockNum);
         if (!node->isSyncMethodMonitor())
            {
            _containsCalls->set(blockNum);
            containsCall = true;
            }

         int32_t lockedObjectValueNumber = info->getValueNumber(node->getFirstChild());
         TR_ASSERT((lockedObjectValueNumber > -1), "Invalid value number for locked object\n");


         if (node->getFirstChild()->getOpCode().hasSymbolReference())
            {
            if (_safeValueNumbers[lockedObjectValueNumber] > 0)
               {
               TR::Node *prevNode = useDefInfo->getNode(_safeValueNumbers[lockedObjectValueNumber]);
               if (prevNode->getSymbolReference()->getReferenceNumber() != node->getFirstChild()->getSymbolReference()->getReferenceNumber())
                  _safeValueNumbers[lockedObjectValueNumber] = -2;
               else
                  {
                  defs1.Clear();
                  defs2.Clear();
                  TR_UseDefInfo::BitVector tempDefs(comp()->allocator());
                  useDefInfo->getUseDef(tempDefs, _safeValueNumbers[lockedObjectValueNumber]);
                  if (!tempDefs.IsZero())
                     {
                     TR_UseDefInfo::BitVector tempUses(comp()->allocator());
                     TR_UseDefInfo::BitVector::Cursor cursor(tempDefs);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = useDefInfo->getFirstDefIndex() + (int32_t) cursor;
                        if (defIndex < useDefInfo->getFirstRealDefIndex())
                           continue;

                        TR::Node *defNode = useDefInfo->getNode(defIndex);
                        if (defNode->getOpCode().isLoad())
                           useDefInfo->dereferenceDef(defs1, defIndex, tempUses);
                        else
                           defs1[defIndex] = true;
                        }
                     }

                  TR_UseDefInfo::BitVector tempDefs2(comp()->allocator());
                  useDefInfo->getUseDef(tempDefs2, node->getFirstChild()->getUseDefIndex());
                  if (!tempDefs2.IsZero())
                     {
                     TR_UseDefInfo::BitVector tempUses(comp()->allocator());
                     TR_UseDefInfo::BitVector::Cursor cursor(tempDefs2);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = useDefInfo->getFirstDefIndex() + (int32_t) cursor;
                        if (defIndex < useDefInfo->getFirstRealDefIndex())
                           continue;

                        TR::Node *defNode = useDefInfo->getNode(defIndex);
                        if (defNode->getOpCode().isLoad())
                           useDefInfo->dereferenceDef(defs2, defIndex, tempUses);
                        else
                           defs2[defIndex] = true;
                        }
                     }

                  if (!(defs1 == defs2))
                     _safeValueNumbers[lockedObjectValueNumber] = -2;

                  }
               }
            else if (_safeValueNumbers[lockedObjectValueNumber] == -1)
               _safeValueNumbers[lockedObjectValueNumber] = node->getFirstChild()->getUseDefIndex();
               //_safeValueNumbers[lockedObjectValueNumber] = node->getFirstChild()->getSymbolReference()->getReferenceNumber();
            }


         if (_monentBlockInfo[blockNum] == -2)
            {
            if (trace())
               traceMsg(comp(), "Kill monent in block_%d cause %p\n", blockNum, node);
            _monentBlockInfo[blockNum] = -1;
            }

         currentMonexit = treeTop;
         _monexitTrees[blockNum] = treeTop;
         _monexitBlockInfo[blockNum] = lockedObjectValueNumber;
         }
      }

   if (_multiplyLockedObjects->isEmpty())
      {
      return;
      }

   if (trace())
      {
      traceMsg(comp(), "Guarded virtual call blocks : \n");
      _guardedVirtualCallBlocks->print(comp());
      traceMsg(comp(), "\n");
      }

   _lastTreeTop = NULL;

   _successorInfo = (TR_BitVector **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR_BitVector *));
   memset(_successorInfo, 0, numBlocks * sizeof(TR_BitVector *));
   _predecessorInfo = (TR_BitVector **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR_BitVector *));
   memset(_predecessorInfo, 0, numBlocks * sizeof(TR_BitVector *));

   _blockInfo = (TR::Block **) trMemory()->allocateStackMemory(numBlocks* sizeof(TR::Block *));
   memset(_blockInfo, 0, numBlocks * sizeof(TR::Block *));

   for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
      {
      _successorInfo[node->getNumber()] = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
      _predecessorInfo[node->getNumber()] = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
      _blockInfo[node->getNumber()] = toBlock(node);
      }

   //TR_BitVector *seenPredNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   //TR_BitVector *seenSuccNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   TR_BitVector *visitedNodes = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   _subtraction = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   TR_BitVector *blocksAtCurrentNestingLevel = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   bool containsImproperRegions = markBlocksAtSameNestingLevel(comp()->getFlowGraph()->getStructure(), blocksAtCurrentNestingLevel);

   if (containsImproperRegions)
      {
      return;
      }

   collectPredsAndSuccs(cfg->getStart(), visitedNodes, _predecessorInfo, _successorInfo, &_cfgBackEdges, _loopEntryBlocks, comp());

   _temp = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _intersection = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _inclusiveIntersection = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _closureIntersection = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _scratch = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _scratchForSymbols = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);
   _writtenSymbols = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growable);
   _monexitWorkList = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _adjustedMonentBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _adjustedMonexitBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _matchingMonentBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _matchingMonexitBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _matchingSpecialBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _matchingNormalMonentBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   _matchingNormalMonexitBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);
   //_matchingSpecialMonexitBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   //*_intersection = *seenNodes;

   _coarsenedMonexits = new (trStackMemory()) TR_BitVector(numBlocks, trMemory(), stackAlloc, notGrowable);

   int32_t prevLockedObject = -1;
   TR::Node *prevMonitorNode = NULL;
   treeTop = comp()->getStartTree();
   for (; treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
        {
        blockNum = node->getBlock()->getNumber();
        prevLockedObject = -1;
        prevMonitorNode = NULL;
        }

      if (node->getOpCodeValue() == TR::BBEnd)
        {

         if (trace())
            traceMsg(comp(),"Coarsening: Considering node %p blockNum %d prevLockedObject = %d\n",node,blockNum,prevLockedObject);

        if (prevLockedObject > -1)
           {

        if (trace())
           traceMsg(comp(),"Coarsening: _multiplyLockedObjects->get(prevLockedObject) = %d _safeValueNumbers[prevLockedObject] = %d _coarsendMonexits->get(blockNum) = %d\n"
                 ,_multiplyLockedObjects->get(prevLockedObject),_safeValueNumbers[prevLockedObject],_coarsenedMonexits->get(blockNum));

           if (_multiplyLockedObjects->get(prevLockedObject) &&
               (_safeValueNumbers[prevLockedObject] > 0) &&
               !_coarsenedMonexits->get(blockNum))
              {
              if (trace())
                 traceMsg(comp(), "Try to coarsen monexit in block_%d\n", blockNum);
              coarsenMonitor(blockNum, prevLockedObject, prevMonitorNode);
              }
           }
        }

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCodeValue() == TR::NULLCHK)
        node = node->getFirstChild();

      if (node->getOpCodeValue() == TR::monexit &&
          !_coarsenedMonexits->get(blockNum))
         {
         prevMonitorNode = node;
         prevLockedObject = info->getValueNumber(prevMonitorNode->getFirstChild());
         //dumpOptDetails(comp(), "Block number %d monexit status %d prevLockedObject %d\n", blockNum, _monexitBlockInfo[blockNum], prevLockedObject);

         if (_monexitBlockInfo[blockNum] < 0)
            {
            prevLockedObject = -1;
            prevMonitorNode = NULL;
            }
         }
      }

   if (!_specialBlockInfo.isEmpty())
      {
      TR_BlockCloner cloner(comp()->getFlowGraph(), true, false);
      ListIterator<SpecialBlockInfo> blocksIt(&_specialBlockInfo);
      SpecialBlockInfo *block;

      if (!comp()->getClassesThatShouldNotBeLoaded()->isEmpty() ||
          !comp()->getClassesThatShouldNotBeNewlyExtended()->isEmpty())
         {
         for (block = blocksIt.getFirst(); block != NULL; block = blocksIt.getNext())
            {
            comp()->getFlowGraph()->setStructure(NULL);
            TR::Block *newBlock = cloner.cloneBlocks(block->_succBlock, block->_succBlock);
            newBlock->setIsCold();
            newBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
            block->_clonedBlock = newBlock;
            }

         blocksIt.reset();
         }

      for (block = blocksIt.getFirst(); block != NULL; block = blocksIt.getNext())
         {
         if (trace())
            traceMsg(comp(), "Coarsening for special block_%d\n", block->_succBlock->getNumber());
         coarsenSpecialBlock(block);
         }
      }

   treeTop = comp()->getStartTree();
   bool coarseningDone = false;
   if (_coarsenedMonitors->elementCount() > 0)
      coarseningDone = true;
   for (; treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCodeValue() == TR::NULLCHK)
        node = node->getFirstChild();

      if ((node->getOpCodeValue() == TR::monexit) ||
          (node->getOpCodeValue() == TR::monent))
         {
         if (coarseningDone)
            node->setSkipSync(false);
         TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
         if (_coarsenedMonitors->get(info->getValueNumber(node->getFirstChild())))
            node->setReadMonitor(false);
         }
      }

   if (coarseningDone && comp()->syncsMarked())
      requestOpt(OMR::globalValuePropagation);

   splitEdgesAndAddMonitors();
   addCatchBlocks();
   }





void TR::MonitorElimination::coarsenMonitor(int32_t origBlockNum, int32_t prevLockedObject, TR::Node *prevMonitorNode)
   {
   // Try to coarsen this lock now
   //
   TR::Block *block = NULL;
   _monexitWorkList->empty();
   _monexitWorkList->set(origBlockNum);

   //_specialBlockInfo.deleteAll();
   //_matchingSpecialBlocks->empty();
   _classesThatShouldNotBeLoaded.setFirst(NULL);
   _classesThatShouldNotBeNewlyExtended.setFirst(NULL);

   while (!_monexitWorkList->isEmpty())
      {
      TR_BitVectorIterator monexitIt(*_monexitWorkList);
      int32_t blockNum = monexitIt.getFirstElement();

      _monexitWorkList->reset(blockNum);

      TR_BitVector *successors = _successorInfo[blockNum];
      *_temp = *successors;

      block = _blockInfo[blockNum];

      if (trace())
         {
         traceMsg(comp(), "Successors of block_%d\n", blockNum);

         _temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *_temp &= *(getBlocksAtSameNestingLevel(block));
      *_temp &= *_containsMonents;

      if (trace())
         {
         traceMsg(comp(), "Blocks At Same Nesting Level\n");
         getBlocksAtSameNestingLevel(block)->print(comp());
         traceMsg(comp(), "\nBlocks containing monents\n");
         _containsMonents->print(comp());
         traceMsg(comp(), "\nSuccessors containing monents (possible coarsening candidates) :\n");
         _temp->print(comp());
         traceMsg(comp(), "\n");
         }

      _matchingMonentBlocks->empty();
      _matchingMonexitBlocks->empty();
      _matchingMonexitBlocks->set(blockNum);
      _closureIntersection->empty();

      _monexitEdges.deleteAll();
      _monentEdges.deleteAll();

      int32_t numSpecialBlocks = 0;
      if (!_specialBlockInfo.isEmpty())
         numSpecialBlocks = _specialBlockInfo.getSize();

      buildClosure(blockNum, _temp, successors, prevLockedObject);

      if (trace())
         traceMsg(comp(), "Finished building closure\n");
      if (_matchingMonentBlocks->isEmpty())
         {
         if (trace())
            traceMsg(comp(), "Returned matching monent\n");
         return;
         }

      if (_specialBlockInfo.getSize() > numSpecialBlocks)
         {
         if (trace())
            traceMsg(comp(), "Special block exists\n");

         //ListIterator<SpecialBlockInfo> blocksIt(&_specialBlockInfo);
         //SpecialBlockInfo *block;
         //for (block = blocksIt.getFirst(); block != NULL; block = blocksIt.getNext())
         //   {
         //   dumpOptDetails(comp(), "Coarsening for special block_%d\n", block->_succBlock->getNumber());
         //   coarsenSpecialBlock(block);
         //   }
         }
      else
         {
         *_inclusiveIntersection = *_closureIntersection;
         *_inclusiveIntersection |= *_matchingMonexitBlocks;
         *_inclusiveIntersection |= *_matchingMonentBlocks;

         bool nodesInList = true;
         TR_BitVectorIterator listIt(*_matchingMonexitBlocks);
         while (listIt.hasMoreElements() &&
                nodesInList)
            {
            int32_t nextListElement = listIt.getNextElement();
            block = _blockInfo[nextListElement];

            bool controlFlowInCatch = false;

            if (!prevMonitorNode->isSyncMethodMonitor() &&
                !block->getExceptionSuccessors().empty())
               {
               nodesInList = false;
               if (block->getExceptionSuccessors().size() == 1)
                  {
                  TR::Block *catchBlock = toBlock(block->getExceptionSuccessors().front()->getTo());
                  nodesInList = catchBlock->isSynchronizedHandler();
                  if (nodesInList)
                     controlFlowInCatch = (catchBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch());
                  }
               }

            if (!nodesInList)
               {
               if (trace())
                  traceMsg(comp(), "Cannot coarsen monexit in block_%d because of unknown catch block structure\n", nextListElement);
               return;
               }
            else
               {
               //TR_SuccessorIterator succIt(block);
               nodesInList = checkIfSuccsInList(block->getSuccessors(), _inclusiveIntersection, controlFlowInCatch, block);
               if (nodesInList)
                  {
                  nodesInList = checkIfSuccsInList(block->getExceptionSuccessors(), _inclusiveIntersection, controlFlowInCatch, block);
                  }
               }

            if (!nodesInList)
               {
               if (trace())
                  traceMsg(comp(), "Cannot coarsen monexit in block_%d because succs of block_%d are NOT in list\n", origBlockNum, nextListElement);
               return;
               }
            }

         if (trace())
            {
            traceMsg(comp(), "Closure of common preds and succs : \n");
            _closureIntersection->print(comp());
            traceMsg(comp(), "\n");
            }

         listIt.setBitVector(*_closureIntersection);
         while (listIt.hasMoreElements())
            {
            int32_t nextListElement = listIt.getNextElement();
            block = _blockInfo[nextListElement];

            //TR_PredecessorIterator predIt(block);
            nodesInList = checkIfPredsInList(block->getPredecessors(), _inclusiveIntersection);
            if (nodesInList)
               {
               //TR_SuccessorIterator succIt(block);
               nodesInList = checkIfSuccsInList(block->getSuccessors(), _inclusiveIntersection);
               if (!nodesInList)
                  {
                  if (trace())
                    traceMsg(comp(), "Cannot coarsen monexit in block_%d because succ test failed for block_%d\n", origBlockNum, nextListElement);
                  break;
                  }
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "Cannot coarsen monexit in block_%d because pred test failed for block_%d\n", origBlockNum, nextListElement);
               break;
               }
            }

         if (!nodesInList)
            return;

         TR_BitVectorIterator succs(*_matchingMonentBlocks);
         while (succs.hasMoreElements() &&
                nodesInList)
            {
            int32_t nextSucc = succs.getNextElement();

            if (trace())
               {
               traceMsg(comp(), "0Block number %d monent status %d prevLockedObject %d\n", nextSucc, _monentBlockInfo[nextSucc], prevLockedObject);
               traceMsg(comp(), "_nodesInList %p\n", nodesInList);
               }

            if (nodesInList)
               {
               block = _blockInfo[nextSucc];
               //TR_PredecessorIterator predIt(block);
               nodesInList = checkIfPredsInList(block->getPredecessors(), _inclusiveIntersection);
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "Cannot coarsen monexit in block_%d because preds of succ %d are NOT in list\n", origBlockNum, nextSucc);
               }

            if (!nodesInList)
               {
               if (trace())
                  traceMsg(comp(), "Cannot coarsen monexit in block_%d because of matching monent in block_%d\n", origBlockNum, nextSucc);
               }
            }

         if (nodesInList)
            {
            //
            // All nodes in list; can coarsen
            //
            *_scratch = *_closureIntersection;
            *_scratch &= *_guardedVirtualCallBlocks;

            // Check if this is a virtual call block as we usually expect
            //
            TR_BitVectorIterator scratchIt(*_scratch);
            while (scratchIt.hasMoreElements())
               {
               int32_t nextBlockNum = scratchIt.getNextElement();
               if (trace())
                  traceMsg(comp(), "Look at virtual call block %d\n", nextBlockNum);

               TR::Block *nextBlock = _blockInfo[nextBlockNum];
               for (auto succEdge = nextBlock->getSuccessors().begin(); succEdge != nextBlock->getSuccessors().end(); ++succEdge)
                  {
                  if (trace())
                     traceMsg(comp(), "Virtual call block %d and succ block %d\n", nextBlockNum, (*succEdge)->getTo()->getNumber());

                  if (!_inclusiveIntersection->get((*succEdge)->getTo()->getNumber()))
                     {
                     if (trace())
                         traceMsg(comp(), "Reset virtual call block %d\n", nextBlockNum);
                     _scratch->reset(nextBlockNum);
                     break;
                     }
                  }
               }

            if (trace())
               {
               traceMsg(comp(), "Guarded virtual call blocks to be adjusted : \n");
               _guardedVirtualCallBlocks->print(comp());
               traceMsg(comp(), "\n");
               }

            TR::CoarsenedMonitorInfo *monitorInfo = findOrCreateCoarsenedMonitorInfo(prevLockedObject, prevMonitorNode);

            ListIterator<TR::CFGEdge> edgesIt(&_monentEdges);
            TR::CFGEdge *edge;
            for (edge = edgesIt.getFirst(); edge != NULL; edge = edgesIt.getNext())
               monitorInfo->addMonentEdge(edge);

            edgesIt.set(&_monexitEdges);
            for (edge = edgesIt.getFirst(); edge != NULL; edge = edgesIt.getNext())
               monitorInfo->addMonexitEdge(edge);

            adjustMonentAndMonexitBlocks(prevMonitorNode, _scratch, prevLockedObject);
            adjustMonexitBlocks(prevMonitorNode, prevLockedObject);
            adjustMonentBlocks(prevMonitorNode, prevLockedObject);

            // Prevent next monitor from being coarsened across this range
            // other wise two monitor ranges intersect where they should NOT
            // avoid potential deadlock
            //
            *_containsCalls |= *_closureIntersection;

            *_scratch = *_closureIntersection;
            *_scratch &= *_containsExceptions;
            monitorInfo->setInterveningBlocks(_scratch);

            succs.setBitVector(*_matchingMonexitBlocks);
            while (succs.hasMoreElements())
               {
               int32_t nextSucc = succs.getNextElement();
               if (performTransformation(comp(), "%s Success: Coarsening monexit in block_%d\n", OPT_DETAILS, nextSucc))
                  {
                  _invalidateUseDefInfo = true;
                  _invalidateValueNumberInfo = true;
                  block = _blockInfo[nextSucc];
                  removeLastMonexitInBlock(block);
                  _coarsenedMonexits->set(nextSucc);

                  if (trace())
                     traceMsg(comp(), "Success: Coarsening monexit in block_%d in %s\n", nextSucc, comp()->signature());
                  }
               }

            succs.setBitVector(*_matchingMonentBlocks);
            while (succs.hasMoreElements())
               {
               int32_t nextSucc = succs.getNextElement();
               if (performTransformation(comp(), "%s Success: Coarsening monent in block_%d\n", OPT_DETAILS, nextSucc))
                  {
                  _invalidateUseDefInfo = true;
                  _invalidateValueNumberInfo = true;
                  block = _blockInfo[nextSucc];
                  removeFirstMonentInBlock(block);

                  if (trace())
                     traceMsg(comp(), "Success: Coarsening monent in block_%d in %s\n", nextSucc, comp()->signature());
                  }
               }
            }
         }
      }
   }


static bool sideEntranceOrExitExists(TR_BitVector *intersection, TR::Block **blockInfo, int32_t predNum, int32_t succNum)
   {
   TR::Block *first = NULL;
   TR::CFGEdge *edge;

   /*
   first = blockInfo[predNum];
   ListIterator<TR::CFGEdge> bi(&first->getSuccessors());
   for (edge = bi.getFirst(); edge != NULL; edge = bi.getNext())
     {
     if (!intersection->get(edge->getTo()->getNumber()) &&
         (edge->getTo()->getNumber() != succNum))
        {
        return true;
        }
     }
    */


   first = blockInfo[succNum];
   for (auto edge = first->getPredecessors().begin(); edge != first->getPredecessors().end(); ++edge)
     {
     if (!intersection->get((*edge)->getFrom()->getNumber()) &&
         ((*edge)->getFrom()->getNumber() != predNum))
        {
        return true;
        }
     }

   TR_BitVectorIterator intersectIt(*intersection);
   while (intersectIt.hasMoreElements())
      {
      int32_t nextBlockNum = intersectIt.getNextElement();

      if ((nextBlockNum == predNum) || (nextBlockNum == succNum))
         continue;

      TR::Block *nextBlock = blockInfo[nextBlockNum];
      for (auto edge = nextBlock->getSuccessors().begin(); edge != nextBlock->getSuccessors().end(); ++edge)
         {
         if (!intersection->get((*edge)->getTo()->getNumber()) &&
             ((*edge)->getTo()->getNumber() != succNum))
            {
            return true;
            }
         }

      for (auto edge = nextBlock->getPredecessors().begin(); edge != nextBlock->getPredecessors().end(); ++edge)
         {
         if (!intersection->get((*edge)->getFrom()->getNumber()) &&
             ((*edge)->getFrom()->getNumber() != predNum))
            {
            return true;
            }
         }
      }

   return false;
   }



void TR::MonitorElimination::buildClosure(int32_t blockNum, TR_BitVector *monentBlocks, TR_BitVector *successors, int32_t lockedObject)
   {
   collectSuccessors(blockNum, monentBlocks, successors, lockedObject);
   }




void TR::MonitorElimination::collectSuccessors(int32_t blockNum, TR_BitVector *monentBlocks, TR_BitVector *successors, int32_t lockedObject)
   {
   bool peekedCall = false;
   bool recoveryPossible = true;
   bool blockContainsCheck = false;
   if (_containsCalls->get(blockNum))
      {
      TR::Block *block = _blockInfo[blockNum];
      recoveryPossible = treesAllowCoarsening(_monexitTrees[blockNum]->getNextTreeTop(), block->getExit(), &peekedCall, &blockContainsCheck);
      if (recoveryPossible)
         {
           //if (peekedCall)
           // printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
         }
      else
         return;
      }

   TR_BitVectorIterator succs(*monentBlocks);
   while (succs.hasMoreElements())
      {
      int32_t nextSucc = succs.getNextElement();
      bool canRecover = false;

      if (trace())
         {
         traceMsg(comp(), "1Block number %d monent status %d prevLockedObject %d\n", nextSucc, _monentBlockInfo[nextSucc], lockedObject);
         traceMsg(comp(), "_monentBlockInfo %d _monentBlockInfo %d\n", _monentBlockInfo[nextSucc], lockedObject);

         }

      if (_monentBlockInfo[nextSucc] != -1 &&
          _monentBlockInfo[nextSucc] == lockedObject)
         {
         //
         //Found a possible monent to coarsen with
         //
         TR_BitVector *predecessors = _predecessorInfo[nextSucc];
         if (trace())
            {
            traceMsg(comp(), "Predecessors for block_%d\n", nextSucc);
            predecessors->print(comp());
            traceMsg(comp(), "\n");
            }

         *_scratch = *successors;
         *_scratch -= *_successorInfo[nextSucc];
         *_intersection = *predecessors;
         *_intersection &= *_scratch;

          bool excCoarsening = false;
     if (_blockInfo[nextSucc]->isCatchBlock())
             excCoarsening = true;
          else
        {
             TR_BitVectorIterator intersectIt(*_intersection);
             while (intersectIt.hasMoreElements())
                {
                int32_t nextBlock = intersectIt.getNextElement();
                //dumpOptDetails(comp(), "Collect succ for block_%d succ %d next %d\n", blockNum, nextSucc, nextBlock);
                if (_blockInfo[nextBlock]->isCatchBlock())
              {
         excCoarsening = true;
         break;
         }
           }
        }

     if (excCoarsening)
        {
        //dumpOptDetails(comp(), "Exc coarsening\n");
        continue;
        }
          //else
          //   dumpOptDetails(comp(), "No exc coarsening\n");



         *_scratch = *predecessors;
         *_scratch -= *_predecessorInfo[blockNum];
         _scratch->reset(blockNum);

         bool succPostDominatesPred = false;
         if (*_intersection == *_scratch)
            {
            if (!sideEntranceOrExitExists(_intersection, _blockInfo, blockNum, nextSucc))
               succPostDominatesPred = true;
            }

         *_subtraction = *_intersection;

         if (trace())
            {
            traceMsg(comp(), "Intersection for block_%d\n", nextSucc);
            _intersection->print(comp());
            traceMsg(comp(), "\n");
            traceMsg(comp(), "Subtraction for block_%d\n", nextSucc);
            _subtraction->print(comp());
            traceMsg(comp(), "\n");
            traceMsg(comp(), "ContainsCalls for block_%d\n", nextSucc);
            _containsCalls->print(comp());
            traceMsg(comp(), "\n");
            }

         *_subtraction -= *_containsMonents;
         *_subtraction -= *_containsMonexits;
         *_subtraction -= *_loopEntryBlocks;

         TR_BitVector *blocksAtSameNestingLevel = getBlocksAtSameNestingLevel(_blockInfo[nextSucc]);

         bool peekedCallInSucc = false;
         bool recoveryPossibleInSucc = true;

         bool succContainsCheck = false;
         if (*_subtraction == *_intersection)
            {
            *_subtraction -= *_containsCalls;

            bool interveningBlocksContainCalls = true;
            if (*_subtraction == *_intersection)
               interveningBlocksContainCalls = false;

            if (_containsCalls->get(nextSucc))
               recoveryPossibleInSucc = treesAllowCoarsening(_blockInfo[nextSucc]->getEntry(), _monentTrees[nextSucc]->getPrevTreeTop(), &peekedCallInSucc, &succContainsCheck);

            if (!interveningBlocksContainCalls &&
                !blockContainsCheck && !succContainsCheck &&
                recoveryPossibleInSucc && !peekedCallInSucc &&
                recoveryPossible && !peekedCall)
               {
               *_scratch = *_containsAsyncCheck;
              // *_scratch -= *blocksAtSameNestingLevel;
               *_subtraction -= *_scratch;
               if (!(*_subtraction == *_intersection))
                  {
                  //
                  // Loops in coarsened range are the only reason we
                  // could not coarsen; check if the locked code is simple (no side-effects).
                  // In that case we can merge the code from the locked code
                  // into previous locked region of code.
                  //
                  if (succPostDominatesPred &&
                      isSimpleLockedRegion(_monentTrees[nextSucc]))
                     {
                     if (symbolsAreNotWritten(_intersection))
                        {
                        if (symbolsAreNotWrittenInTrees(_monexitTrees[blockNum]->getNextTreeTop(), _blockInfo[blockNum]->getExit()) &&
                            symbolsAreNotWrittenInTrees(_blockInfo[nextSucc]->getEntry(), _monentTrees[nextSucc]->getPrevTreeTop()))
                           {
                           canRecover = true;
                           if (trace())
                              {
                              traceMsg(comp(),"Found a coarsening opportunity across loop\n");
                              //printf("Found a coarsening opportunity across loop in %s\n", comp()->signature());
                              }
                           }
                        }
                     }
                  }
               }
            else if (recoveryPossibleInSucc && recoveryPossible)
               {
               if (trace() &&(peekedCallInSucc || peekedCall))
                  {
                  traceMsg(comp(),"Found a coarsening opportunity across call (peek done successfully)\n");
                  //printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
                  }

               //
               // Compensate for calls
               //
               if (succPostDominatesPred &&
                   isSimpleLockedRegion(_monentTrees[nextSucc]))
                  {
                  bool recoveryPossibleInInterveningBlocks = true;
                  if (interveningBlocksContainCalls)
                     {
                     *_scratch = *_intersection;
                     *_scratch -= *_subtraction;

                     recoveryPossibleInInterveningBlocks = callsAllowCoarsening();
                     }

                  if (recoveryPossibleInInterveningBlocks &&
                      peekedCall)
                     {
                     bool peekingDone = false;
                     TR::Block *block = _blockInfo[blockNum];
                     recoveryPossibleInInterveningBlocks = treesAllowCoarsening(_monexitTrees[blockNum]->getNextTreeTop(), block->getExit(), &peekingDone);
                     }

                  if (recoveryPossibleInInterveningBlocks &&
                      peekedCallInSucc)
                     {
                     bool peekingDone = false;
                     recoveryPossibleInInterveningBlocks = treesAllowCoarsening(_blockInfo[nextSucc]->getEntry(), _monentTrees[nextSucc]->getPrevTreeTop(), &peekingDone);
                     }

                  if (recoveryPossibleInInterveningBlocks)
                      {
                      if (symbolsAreNotWritten(_intersection) &&
                          symbolsAreNotWrittenInTrees(_monexitTrees[blockNum]->getNextTreeTop(), _blockInfo[blockNum]->getExit()) &&
                          symbolsAreNotWrittenInTrees(_blockInfo[nextSucc]->getEntry(), _monentTrees[nextSucc]->getPrevTreeTop()))
                         {
                         canRecover = true;
                         if (trace())
                            {
                            traceMsg(comp(),"Found a coarsening opportunity across call (peek done successfully)\n");
                            //printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
                            }
                         }
                      }
                  }
               }
            }

         bool simpleCoarsening = false;
         if ((*_subtraction == *_intersection) &&
             !blockContainsCheck &&
             !succContainsCheck &&
             !peekedCall &&
             !peekedCallInSucc)
            simpleCoarsening = true;

         //dumpOptDetails(comp(), "simpleCoarsening %d canRecover %d blockNum %d nextSucc %d\n", simpleCoarsening, canRecover, blockNum, nextSucc);
         //if (canRecover)
         //   {
         //   _matchingMonentBlocks->print(comp());
         //   _matchingMonexitBlocks->print(comp());
         //   }

         if ((simpleCoarsening && !_matchingSpecialBlocks->get(blockNum) && !_matchingSpecialBlocks->get(nextSucc)) ||
             (canRecover && !_matchingNormalMonentBlocks->get(nextSucc) && !_matchingNormalMonexitBlocks->get(blockNum)))
            {
            if (!simpleCoarsening)
               {
               _matchingSpecialBlocks->set(nextSucc);

               SpecialBlockInfo *specialBlockInfo = (SpecialBlockInfo *) trMemory()->allocateStackMemory(sizeof(SpecialBlockInfo));
               //specialBlockInfo->_predBlock = _blockInfo[blockNum];
               specialBlockInfo->_succBlock = _blockInfo[nextSucc];
               if (trace())
                  traceMsg(comp(), "special block info added\n");
               specialBlockInfo->_treeTop = _monexitTrees[blockNum]->getPrevTreeTop();
               specialBlockInfo->_clonedBlock = NULL;
               _specialBlockInfo.add(specialBlockInfo);

               for (TR_ClassLoadCheck * clc = _classesThatShouldNotBeLoaded.getFirst(); clc; clc = clc->getNext())
                  addClassThatShouldNotBeLoaded(clc->_name, clc->_length, comp()->getClassesThatShouldNotBeLoaded(), false);

               for (TR_ClassExtendCheck * cec = _classesThatShouldNotBeNewlyExtended.getFirst(); cec; cec = cec->getNext())
                  addClassThatShouldNotBeNewlyExtended(cec->_clazz, comp()->getClassesThatShouldNotBeNewlyExtended(), false);
               }
            else
               {
               _matchingNormalMonexitBlocks->set(blockNum);
               _matchingNormalMonentBlocks->set(nextSucc);

               *_closureIntersection |= *_intersection;

               if (!_matchingMonentBlocks->get(nextSucc))
                  {
                  _matchingMonentBlocks->set(nextSucc);
                  TR_BitVector *predsContainingMonexits = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, notGrowable);
                  *predsContainingMonexits = *predecessors;
                  *predsContainingMonexits &= *blocksAtSameNestingLevel;
                  *predsContainingMonexits &= *_containsMonexits;
                  collectPredecessors(nextSucc, predsContainingMonexits, predecessors, lockedObject);
                  }
               }
            }
         }
      }
   }






void TR::MonitorElimination::collectPredecessors(int32_t blockNum, TR_BitVector *monexitBlocks, TR_BitVector *predecessors, int32_t lockedObject)
   {
   bool peekedCall = false;
   bool recoveryPossible = true;
   bool blockContainsCheck = false;
   if (_containsCalls->get(blockNum))
      {
      TR::Block *block = _blockInfo[blockNum];
      recoveryPossible = treesAllowCoarsening(block->getEntry(), _monentTrees[blockNum]->getPrevTreeTop(), &peekedCall, &blockContainsCheck);
      if (recoveryPossible)
         {
           //printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
         }
      else
         return;
      }

   TR_BitVectorIterator preds(*monexitBlocks);
   while (preds.hasMoreElements())
      {
      int32_t nextPred = preds.getNextElement();
      bool canRecover = false;

      if (trace())
         traceMsg(comp(), "Block number %d monexit status %d prevLockedObject %d\n", nextPred, _monexitBlockInfo[nextPred], lockedObject);

      if (_monexitBlockInfo[nextPred] != -1 &&
          _monexitBlockInfo[nextPred] == lockedObject)
         {
         //
         //Found a possible monent to coarsen with
         //
         TR_BitVector *successors = _successorInfo[nextPred];
         if (trace())
            {
            traceMsg(comp(), "Successors for block_%d\n", nextPred);
            successors->print(comp());
            traceMsg(comp(), "\n");
            }

         *_scratch = *predecessors;
         *_scratch -= *_predecessorInfo[nextPred];
         *_intersection = *successors;
         *_intersection &= *_scratch;

     bool excCoarsening = false;
     if (_blockInfo[blockNum]->isCatchBlock())
             excCoarsening = true;
          else
        {
             TR_BitVectorIterator intersectIt(*_intersection);
             while (intersectIt.hasMoreElements())
                {
                int32_t nextBlock = intersectIt.getNextElement();
                //dumpOptDetails(comp(), "Collect pred for block_%d succ %d next %d\n", blockNum, nextPred, nextBlock);
                if (_blockInfo[nextBlock]->isCatchBlock())
              {
         excCoarsening = true;
         break;
         }
           }
        }

          if (excCoarsening)
        {
        //dumpOptDetails(comp(), "Exc coarsening\n");
        continue;
        }
          //else
          //   dumpOptDetails(comp(), "No exc coarsening\n");


         _scratch->reset(nextPred);
         bool succPostDominatesPred = false;
         if (*_intersection == *_scratch)
            {
            if (!sideEntranceOrExitExists(_intersection, _blockInfo, nextPred, blockNum))
               succPostDominatesPred = true;
            }

         *_subtraction = *_intersection;

         if (trace())
            {
            traceMsg(comp(), "Intersection for block_%d\n", nextPred);
            _intersection->print(comp());
            traceMsg(comp(), "\n");
            traceMsg(comp(), "Subtraction for block_%d\n", nextPred);
            _subtraction->print(comp());
            traceMsg(comp(), "\n");
            traceMsg(comp(), "ContainsCalls for block_%d\n", nextPred);
            _containsCalls->print(comp());
            traceMsg(comp(), "\n");
            }

         *_subtraction -= *_containsMonents;
         *_subtraction -= *_containsMonexits;
         *_subtraction -= *_loopEntryBlocks;

         TR_BitVector *blocksAtSameNestingLevel = getBlocksAtSameNestingLevel(_blockInfo[nextPred]);

         bool peekedCallInPred = false;
         bool recoveryPossibleInPred = true;
         bool predContainsCheck = false;

         if (*_subtraction == *_intersection)
            {
            *_subtraction -= *_containsCalls;

            bool interveningBlocksContainCalls = true;
            if (*_subtraction == *_intersection)
               interveningBlocksContainCalls = false;

            if (_containsCalls->get(nextPred))
               recoveryPossibleInPred = treesAllowCoarsening(_monexitTrees[nextPred]->getNextTreeTop(), _blockInfo[nextPred]->getExit(), &peekedCallInPred, &predContainsCheck);

            if (!interveningBlocksContainCalls &&
                !blockContainsCheck && !predContainsCheck &&
                recoveryPossibleInPred && !peekedCallInPred &&
                recoveryPossible && !peekedCall)
               {
               *_scratch = *_containsAsyncCheck;
               //*_scratch -= *blocksAtSameNestingLevel;
               *_subtraction -= *_scratch;
               if (!(*_subtraction == *_intersection))
                  {
                  //
                  // Loops in coarsened range are the only reason we
                  // could not coarsen; check if the locked code is simple (no side-effects).
                  // In that case we can merge the code from the locked code
                  // into previous locked region of code.
                  //
                  if (succPostDominatesPred &&
                      isSimpleLockedRegion(_monexitTrees[nextPred]))
                     {
                     if (symbolsAreNotWritten(_intersection) &&
                         symbolsAreNotWrittenInTrees(_monexitTrees[nextPred]->getNextTreeTop(), _blockInfo[nextPred]->getExit()) &&
                         symbolsAreNotWrittenInTrees(_blockInfo[blockNum]->getEntry(), _monentTrees[blockNum]->getPrevTreeTop()))
                        {
                        canRecover = true;
                        if (trace())
                           {
                            traceMsg(comp(),"Found a coarsening opportunity across loop\n");
                            //printf("Found a coarsening opportunity across loop in %s\n", comp()->signature());
                           }
                        }
                     }
                  }
               }
            else if (recoveryPossibleInPred && recoveryPossible)
               {
               if (trace() && (peekedCallInPred || peekedCall))
                  {
                  traceMsg(comp(),"Found a coarsening opportunity across call (peek done successfully");
                  //printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
                  }
               //
               // Compensate for calls
               //
               if (succPostDominatesPred &&
                   isSimpleLockedRegion(_monexitTrees[nextPred]))
                  {
                  bool recoveryPossibleInInterveningBlocks = true;
                  if (interveningBlocksContainCalls)
                     {
                     *_scratch = *_intersection;
                     *_scratch -= *_subtraction;

                     recoveryPossibleInInterveningBlocks = callsAllowCoarsening();
                     }

                  if (recoveryPossibleInInterveningBlocks &&
                      peekedCall)
                     {
                     bool peekingDone = false;
                     TR::Block *block = _blockInfo[blockNum];
                     recoveryPossibleInInterveningBlocks = treesAllowCoarsening(block->getEntry(), _monentTrees[blockNum]->getPrevTreeTop(), &peekingDone);
                     }

                   if (recoveryPossibleInInterveningBlocks &&
                      peekedCallInPred)
                     {
                     bool peekingDone = false;
                     recoveryPossibleInInterveningBlocks = treesAllowCoarsening(_monexitTrees[nextPred]->getNextTreeTop(), _blockInfo[nextPred]->getExit(), &peekingDone);
                     }

                  if (recoveryPossibleInInterveningBlocks)
                     {
                     if (symbolsAreNotWritten(_intersection) &&
                         symbolsAreNotWrittenInTrees(_monexitTrees[nextPred]->getNextTreeTop(), _blockInfo[nextPred]->getExit()) &&
                         symbolsAreNotWrittenInTrees(_blockInfo[blockNum]->getEntry(), _monentTrees[blockNum]->getPrevTreeTop()))
                         {
                         canRecover = true;
                         if (trace())
                            {
                            traceMsg(comp(),"Found a coarsening opportunity across call (peek done successfully)\n");
                            //printf("Found a coarsening opportunity across call (peek done successfully) in %s\n", comp()->signature());
                            }
                         }
                      }
                  }
               }
            }

         bool simpleCoarsening = false;
         if ((*_subtraction == *_intersection) &&
             !blockContainsCheck &&
             !predContainsCheck &&
             !peekedCall &&
             !peekedCallInPred)
            simpleCoarsening = true;

         //dumpOptDetails(comp(), "simpleCoarsening %d canRecover %d blockNum %d nextPred %d\n", simpleCoarsening, canRecover, blockNum, nextPred);
         //if (canRecover)
         //   {
         //   _matchingMonentBlocks->print(comp());
         //   _matchingMonexitBlocks->print(comp());
         //   }

         if ((simpleCoarsening && !_matchingSpecialBlocks->get(blockNum) && !_matchingSpecialBlocks->get(nextPred)) ||
             (canRecover && !_matchingNormalMonentBlocks->get(blockNum) && !_matchingNormalMonexitBlocks->get(nextPred)))
            {
            if (!simpleCoarsening)
               {
               _matchingSpecialBlocks->set(blockNum);

               if (trace())
                  traceMsg(comp(), "1special block info added\n");
               SpecialBlockInfo *specialBlockInfo = (SpecialBlockInfo *) trMemory()->allocateStackMemory(sizeof(SpecialBlockInfo));
               //specialBlockInfo->_predBlock = _blockInfo[nextPred];
               specialBlockInfo->_succBlock = _blockInfo[blockNum];
               specialBlockInfo->_treeTop = _monexitTrees[nextPred]->getPrevTreeTop();
               specialBlockInfo->_clonedBlock = NULL;
               _specialBlockInfo.add(specialBlockInfo);

               for (TR_ClassLoadCheck * clc = _classesThatShouldNotBeLoaded.getFirst(); clc; clc = clc->getNext())
                  addClassThatShouldNotBeLoaded(clc->_name, clc->_length, comp()->getClassesThatShouldNotBeLoaded(), false);

               for (TR_ClassExtendCheck * cec = _classesThatShouldNotBeNewlyExtended.getFirst(); cec; cec = cec->getNext())
                  addClassThatShouldNotBeNewlyExtended(cec->_clazz, comp()->getClassesThatShouldNotBeNewlyExtended(), false);
               }
            else
               {
               _matchingNormalMonexitBlocks->set(nextPred);
               _matchingNormalMonentBlocks->set(blockNum);

               *_closureIntersection |= *_intersection;

               if (!_matchingMonexitBlocks->get(nextPred))
                  {
                  _matchingMonexitBlocks->set(nextPred);
                  TR_BitVector *succsContainingMonents = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, notGrowable);
                  *succsContainingMonents = *successors;
                  *succsContainingMonents &= *(getBlocksAtSameNestingLevel(_blockInfo[nextPred]));
                  *succsContainingMonents &= *_containsMonents;
                  collectSuccessors(nextPred, succsContainingMonents, successors, lockedObject);
                  }
               }
            }
         }
      }
   }



TR_BitVector *TR::MonitorElimination::getBlocksAtSameNestingLevel(TR::Block *block)
  {
  TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
  TR_RegionStructure *structure = block->getStructureOf()->getParent()->asRegion();
  while (structure &&
         !structure->asRegion()->isNaturalLoop() &&
         (structure != rootStructure))
     structure = structure->getParent()->asRegion();

  return structure->getBlocksAtSameNestingLevel();
  }



void TR::MonitorElimination::adjustMonentAndMonexitBlocks(TR::Node *prevMonitorNode, TR_BitVector *blocks, int32_t lockedObject)
   {
   TR::CoarsenedMonitorInfo *monitorInfo = findOrCreateCoarsenedMonitorInfo(lockedObject, prevMonitorNode);

   TR_BitVectorIterator bvi(*blocks);
   while (bvi.hasMoreElements())
      {
      int32_t nextBlockNum = bvi.getNextElement();
      TR::Block *nextBlock = _blockInfo[nextBlockNum];

      if (trace())
        traceMsg(comp(), "Adding monexit and monent in block_%d\n", nextBlockNum);

      for (auto edge = nextBlock->getPredecessors().begin(); edge != nextBlock->getPredecessors().end(); ++edge)
          monitorInfo->addMonexitEdge(*edge);

      for (auto edge = nextBlock->getSuccessors().begin(); edge != nextBlock->getSuccessors().end(); ++edge)
          monitorInfo->addMonentEdge(*edge);

      //prependMonexitAndAppendMonentInBlock(prevMonitorNode, nextBlock, lockedObject);
      }
   }




void TR::MonitorElimination::prependMonexitAndAppendMonentInBlock(TR::Node *prevMonitorNode, TR::Block *block, int32_t lockedObject)
   {
   prependMonexitInBlock(prevMonitorNode, block, lockedObject, false);
   appendMonentInBlock(prevMonitorNode, block, lockedObject, false);
   }



void TR::MonitorElimination::adjustMonexitBlocks(TR::Node *prevMonitorNode, int32_t lockedObject)
   {
   TR_BitVectorIterator bvi(*_adjustedMonexitBlocks);
   while (bvi.hasMoreElements())
      {
      int32_t nextBlockNum = bvi.getNextElement();
      TR::Block *nextBlock = _blockInfo[nextBlockNum];
      prependMonexitInBlock(prevMonitorNode, nextBlock, lockedObject);
      }
   }


void TR::MonitorElimination::prependMonexitInBlock(TR::Node *prevMonitorNode, TR::Block *nextBlock, int32_t lockedObject, bool insertNullTest)
   {
   TR::CoarsenedMonitorInfo *monitorInfo = findOrCreateCoarsenedMonitorInfo(lockedObject, prevMonitorNode);

   if (!monitorInfo->adjustedMonexitBlock(nextBlock->getNumber()))
      {
      monitorInfo->setAdjustedMonexitBlock(nextBlock->getNumber());
      prependMonexitInBlock(prevMonitorNode, nextBlock, insertNullTest);
      }
   }


void TR::MonitorElimination::prependMonexitInBlock(TR::Node *prevMonitorNode, TR::Block *nextBlock, bool insertNullTest)
   {
   if (trace())
      traceMsg(comp(), "Adding monexit in block_%d\n", nextBlock->getNumber());

   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol *owningMethodSymbol = NULL;

   if (prevMonitorNode->getOpCode().hasSymbolReference())
      owningMethodSymbol = prevMonitorNode->getSymbolReference()->getOwningMethodSymbol(comp());

   _invalidateUseDefInfo = true;
   _invalidateValueNumberInfo = true;

   TR::Node *monexitNode = TR::Node::createWithSymRef(TR::monexit, 1, 1, prevMonitorNode->getFirstChild()->duplicateTree(), symRefTab->findOrCreateMonitorExitSymbolRef(owningMethodSymbol));
   TR::Node *treetopNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, monexitNode, symRefTab->findOrCreateNullCheckSymbolRef(owningMethodSymbol));
   if (treetopNode->getNullCheckReference()->getOpCodeValue() != TR::loadaddr)
      {
      treetopNode->getNullCheckReference()->setIsNonNull(false);
      treetopNode->getNullCheckReference()->setIsNull(false);
      }
   monexitNode->setSyncMethodMonitor(true);

   TR::TreeTop *treetop = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
   TR::TreeTop *lastTree = nextBlock->getLastRealTreeTop();
   TR::Node *lastNode = lastTree->getNode();
   if (lastNode->getOpCode().isReturn())
      {
      TR::TreeTop *prevTree = lastTree->getPrevTreeTop();
      prevTree->join(treetop);
      treetop->join(lastTree);
      }
   else
      nextBlock->prepend(treetop);

   if (insertNullTest)
      insertNullTestBeforeBlock(prevMonitorNode, nextBlock);
   }


void TR::MonitorElimination::adjustMonentBlocks(TR::Node *prevMonitorNode, int32_t lockedObject)
   {
   TR_BitVectorIterator bvi(*_adjustedMonentBlocks);
   while (bvi.hasMoreElements())
      {
      int32_t nextBlockNum = bvi.getNextElement();
      TR::Block *nextBlock = _blockInfo[nextBlockNum];
      appendMonentInBlock(prevMonitorNode, nextBlock, lockedObject);
      }
   }



void TR::MonitorElimination::appendMonentInBlock(TR::Node *prevMonitorNode, TR::Block *nextBlock, int32_t lockedObject, bool insertNullTest)
   {
   TR::CoarsenedMonitorInfo *monitorInfo = findOrCreateCoarsenedMonitorInfo(lockedObject, prevMonitorNode);

   if (!monitorInfo->adjustedMonentBlock(nextBlock->getNumber()))
      {
      monitorInfo->setAdjustedMonentBlock(nextBlock->getNumber());
      appendMonentInBlock(prevMonitorNode, nextBlock, insertNullTest);
      }
   }




void TR::MonitorElimination::appendMonentInBlock(TR::Node *prevMonitorNode, TR::Block *nextBlock, bool insertNullTest)
   {
   if (trace())
      traceMsg(comp(), "Adding monent in block_%d\n", nextBlock->getNumber());

   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol *owningMethodSymbol = NULL;

   if (prevMonitorNode->getOpCode().hasSymbolReference())
      owningMethodSymbol = prevMonitorNode->getSymbolReference()->getOwningMethodSymbol(comp());

   _invalidateUseDefInfo = true;
   _invalidateValueNumberInfo = true;

   TR::Node *monentNode = TR::Node::createWithSymRef(TR::monent, 1, 1, prevMonitorNode->getFirstChild()->duplicateTree(), symRefTab->findOrCreateMonitorEntrySymbolRef(owningMethodSymbol));
   monentNode->setSyncMethodMonitor(true);
   TR::Node *treetopNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, monentNode, symRefTab->findOrCreateNullCheckSymbolRef(owningMethodSymbol));
   if (treetopNode->getNullCheckReference()->getOpCodeValue() != TR::loadaddr)
      {
      treetopNode->getNullCheckReference()->setIsNonNull(false);
      treetopNode->getNullCheckReference()->setIsNull(false);
      }
   TR::TreeTop *treetop = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
   TR::TreeTop *lastTree = nextBlock->getLastRealTreeTop();
   TR::Node *lastNode = lastTree->getNode();
   TR::ILOpCode & lastOp = lastNode->getOpCode();
   if (lastOp.isBranch() || lastOp.isSwitch() || lastOp.isReturn() || lastNode->getOpCodeValue() == TR::athrow)
      {
      lastTree->getPrevTreeTop()->join(treetop);
      treetop->join(nextBlock->getLastRealTreeTop());
      }
   else
      {
      lastTree->join(treetop);
      treetop->join(nextBlock->getExit());
      }

   if (insertNullTest)
      insertNullTestBeforeBlock(prevMonitorNode, nextBlock);
   }



void TR::MonitorElimination::insertNullTestBeforeBlock(TR::Node *prevMonitorNode, TR::Block *nextBlock)
   {
   if (trace())
      traceMsg(comp(), "Inserting null test before block_%d\n", nextBlock->getNumber());

   //printf("Inserting null test in method %s\n", comp()->signature());

   _invalidateUseDefInfo = true;
   _invalidateValueNumberInfo = true;

   TR::Block *destBlock = nextBlock->getSuccessors().front()->getTo()->asBlock();

   TR::Node *ifNode = TR::Node::createif(TR::ifacmpeq, prevMonitorNode->getFirstChild()->duplicateTree(), TR::Node::aconst(prevMonitorNode, 0), destBlock->getEntry());
   if (ifNode->getFirstChild()->getOpCodeValue() != TR::loadaddr)
      {
      ifNode->getFirstChild()->setIsNonNull(false);
      ifNode->getFirstChild()->setIsNull(false);
      }

   TR::TreeTop *ifTree = TR::TreeTop::create(comp(), ifNode, NULL, NULL);
   TR::Block *ifBlock = TR::Block::createEmptyBlock(nextBlock->getEntry()->getNode(), comp(), nextBlock->getFrequency(), nextBlock);
   TR::TreeTop *ifBlockEntry = ifBlock->getEntry();
   TR::TreeTop *ifBlockExit = ifBlock->getExit();
   ifBlockEntry->join(ifTree);
   ifTree->join(ifBlockExit);

   TR::TreeTop *nextBlockEntry = nextBlock->getEntry();
   TR::TreeTop *treePrevToNextBlockEntry = nextBlockEntry->getPrevTreeTop();
   treePrevToNextBlockEntry->join(ifBlockEntry);
   ifBlockExit->join(nextBlockEntry);

   TR::CFG *cfg = comp()->getFlowGraph();
   cfg->addNode(ifBlock);
   _nullTestBlocks.add(ifBlock);

   for (auto edge = nextBlock->getPredecessors().begin(); edge != nextBlock->getPredecessors().end(); ++edge)
      (*edge)->getFrom()->asBlock()->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), nextBlock->getEntry(), ifBlock->getEntry());
   nextBlock->movePredecessors(ifBlock);
   cfg->addEdge(TR::CFGEdge::createEdge(ifBlock,  nextBlock, trMemory()));
   cfg->addEdge(TR::CFGEdge::createEdge(ifBlock,  destBlock, trMemory()));
   }




void TR::MonitorElimination::removeLastMonexitInBlock(TR::Block *block)
   {
   TR::TreeTop *monitorTree = block->getLastRealTreeTop();
   while (monitorTree != block->getEntry())
      {
      TR::Node *monitorNode = monitorTree->getNode();
      if (monitorNode->getOpCode().isNullCheck() ||
          monitorNode->getOpCodeValue() == TR::treetop)
         monitorNode = monitorNode->getFirstChild();

      if (monitorNode->getOpCodeValue() == TR::monexit)
         {
         TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
         TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, monitorNode->getFirstChild());
         TR::Node *treetopNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passThroughNode, symRefTab->findOrCreateNullCheckSymbolRef(monitorNode->getSymbolReference()->getOwningMethodSymbol(comp())));

         if (treetopNode->getNullCheckReference()->getOpCodeValue() != TR::loadaddr)
            {
            treetopNode->getNullCheckReference()->setIsNonNull(false);
            treetopNode->getNullCheckReference()->setIsNull(false);
            }

         TR::TreeTop *nullchkTree = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
         TR::TreeTop *prevTree = monitorTree->getPrevTreeTop();
         prevTree->join(nullchkTree);
         nullchkTree->join(monitorTree);

         //monitorTree->getNode()->recursivelyDecReferenceCount();
         //monitorTree->getPrevTreeTop()->join(monitorTree->getNextTreeTop());
         if (monitorTree->getNode() == monitorNode)
            TR::Node::recreate(monitorTree->getNode(), TR::treetop);
         else
            {
            TR::Node::recreate(monitorNode, TR::PassThrough);
            }

         TR_ValueNumberInfo *info = optimizer()->getValueNumberInfo();
         _coarsenedMonitors->set(info->getValueNumber(monitorNode->getFirstChild()));
         break;
         }

      monitorTree = monitorTree->getPrevTreeTop();
      }
   }



void TR::MonitorElimination::removeFirstMonentInBlock(TR::Block *block)
   {
   TR::TreeTop *monitorTree = block->getFirstRealTreeTop();
   while (monitorTree != block->getExit())
      {
      TR::Node *monitorNode = monitorTree->getNode();
      if (monitorNode->getOpCode().isNullCheck() ||
          monitorNode->getOpCodeValue() == TR::treetop)
         monitorNode = monitorNode->getFirstChild();

      if (monitorNode->getOpCodeValue() == TR::monent)
         {
         TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
         TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, monitorNode->getFirstChild());
         TR::Node *treetopNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passThroughNode, symRefTab->findOrCreateNullCheckSymbolRef(monitorNode->getSymbolReference()->getOwningMethodSymbol(comp())));

         if (treetopNode->getNullCheckReference()->getOpCodeValue() != TR::loadaddr)
            {
            treetopNode->getNullCheckReference()->setIsNonNull(false);
            treetopNode->getNullCheckReference()->setIsNull(false);
            }

         TR::TreeTop *nullchkTree = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
         TR::TreeTop *prevTree = monitorTree->getPrevTreeTop();
         prevTree->join(nullchkTree);
         nullchkTree->join(monitorTree);

         //monitorTree->getNode()->recursivelyDecReferenceCount();
         //monitorTree->getPrevTreeTop()->join(monitorTree->getNextTreeTop());
         if (monitorTree->getNode() == monitorNode)
            TR::Node::recreate(monitorTree->getNode(), TR::treetop);
         else
            {
            TR::Node::recreate(monitorNode, TR::PassThrough);
            }
         break;
         }

      monitorTree = monitorTree->getNextTreeTop();
      }
   }




void TR::MonitorElimination::splitEdgesAndAddMonitors()
   {
   ListElement<TR::CoarsenedMonitorInfo> *listElem = _coarsenedMonitorsInfo.getListHead();
   while (listElem)
      {
      _lastTreeTop = comp()->getMethodSymbol()->getLastTreeTop();
      TR::CoarsenedMonitorInfo *monitorInfo = listElem->getData();
      ListIterator<TR::CFGEdge> edgesIt(&monitorInfo->getMonentEdges());
      TR::CFGEdge *edge;
      for (edge = edgesIt.getFirst(); edge != NULL; edge = edgesIt.getNext())
         {
         comp()->getFlowGraph()->setStructure(NULL);
         TR::Block *from = toBlock(edge->getFrom());
         TR::Block *to = toBlock(edge->getTo());
         TR::Block *splitBlock = findOrSplitEdge(from, to);
         appendMonentInBlock(monitorInfo->getMonitorNode(), splitBlock);
         }

      edgesIt.set(&monitorInfo->getMonexitEdges());
      for (edge = edgesIt.getFirst(); edge != NULL; edge = edgesIt.getNext())
        {
        comp()->getFlowGraph()->setStructure(NULL);
        TR::Block *from = toBlock(edge->getFrom());
        TR::Block *to = toBlock(edge->getTo());
        TR::Block *splitBlock = findOrSplitEdge(from, to);
        prependMonexitInBlock(monitorInfo->getMonitorNode(), splitBlock);
        }

      listElem = listElem->getNextElement();
      }
   }


TR::Block *TR::MonitorElimination::findOrSplitEdge(TR::Block *from, TR::Block *to)
   {
   if (to == comp()->getFlowGraph()->getEnd())
      {
      _splitBlocks.add(from);

      return from;
      }

   TR::Block *splitBlock = NULL;
   if (!from->hasSuccessor(to))
      {
      for (auto edge2 = to->getPredecessors().begin(); edge2 != to->getPredecessors().end(); ++edge2)
         {
         if (_splitBlocks.find((*edge2)->getFrom()))
            {
            TR::Block *fromBlock = toBlock((*edge2)->getFrom());
            if (from->hasSuccessor(fromBlock))
               {
               splitBlock = fromBlock;
            break;
               }

            if (!splitBlock)
               {
               TR::Block *origFromBlock = fromBlock;
               while (fromBlock &&
                      (fromBlock->getPredecessors().size() == 1))
                  {
                  TR::Block *nullTestBlock = toBlock(fromBlock->getPredecessors().front()->getFrom());
                  bool isNullTestBlock = false;
                  if (_nullTestBlocks.find(nullTestBlock))
                     isNullTestBlock = true;

                  if (isNullTestBlock &&
                      from->hasSuccessor(nullTestBlock))
                     {
                     splitBlock = origFromBlock;
                     break;
                     }

                  if (isNullTestBlock)
                     fromBlock = nullTestBlock;
                  else
                     fromBlock = NULL;
                  }

               if (splitBlock)
                  break;
               }
            }
         }
      }
   else
      {
      splitBlock = from->splitEdge(from, to, comp(), &_lastTreeTop);
      _splitBlocks.add(splitBlock);
      }

   return splitBlock;
   }








void TR::MonitorElimination::addCatchBlocks()
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   _lastTreeTop = comp()->getMethodSymbol()->getLastTreeTop();
   ListElement<TR::CoarsenedMonitorInfo> *listElem = _coarsenedMonitorsInfo.getListHead();
   while (listElem)
      {
      TR::CoarsenedMonitorInfo *monitorInfo = listElem->getData();

      if (monitorInfo->getInterveningBlocks().isEmpty())
         {
         listElem = listElem->getNextElement();
         continue;
         }

      cfg->setStructure(NULL);

      TR::SymbolReferenceTable * symRefTab = comp()->getSymRefTab();
      TR::Node *monitorNode = monitorInfo->getMonitorNode();
      TR::Block * catchBlock = TR::Block::createEmptyBlock(monitorNode, comp(), 0);
      catchBlock->setHandlerInfo(0, comp()->getInlineDepth(), 0, comp()->getCurrentMethod(), comp());

      TR::SymbolReference * tempSymRef = symRefTab->createTemporary(comp()->getMethodSymbol(), TR::Address);

      _invalidateUseDefInfo = true;
      _invalidateValueNumberInfo = true;

      TR::Node * excpNode = TR::Node::createWithSymRef(monitorNode, TR::aload, 0, symRefTab->findOrCreateExcpSymbolRef());
      TR::Node * storeNode = TR::Node::createWithSymRef(TR::astore, 1, 1, excpNode, tempSymRef);
      TR::TreeTop *storeTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);
      TR::TreeTop *nextTree = catchBlock->getEntry()->getNextTreeTop();
      catchBlock->getEntry()->join(storeTree);
      storeTree->join(nextTree);
      TR::ResolvedMethodSymbol *owningMethodSymbol = NULL;

      if(monitorNode->getOpCode().hasSymbolReference())
         owningMethodSymbol = monitorNode->getSymbolReference()->getOwningMethodSymbol(comp());

      TR::Node *monexitNode = TR::Node::createWithSymRef(TR::monexit, 1, 1, monitorNode->getFirstChild()->duplicateTree(), symRefTab->findOrCreateMonitorExitSymbolRef(owningMethodSymbol));
      monexitNode->setSyncMethodMonitor(true);
      catchBlock->append(TR::TreeTop::create(comp(), monexitNode));
      TR::Node * temp = TR::Node::createWithSymRef(monitorNode, TR::aload, 0, tempSymRef);
      catchBlock->append(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::athrow, 1, 1, temp, symRefTab->findOrCreateAThrowSymbolRef(comp()->getMethodSymbol()))));

//      _lastTreeTop->join(catchBlock->getEntry());
//      _lastTreeTop = catchBlock->getExit();

      if (trace())
          traceMsg(comp(), "Created catch block_%d(%p)\n", catchBlock->getNumber(), catchBlock);

      bool firstTime = true, addedNode = false;
      TR_BitVectorIterator listIt(monitorInfo->getInterveningBlocks());
      while (listIt.hasMoreElements())
         {
         int32_t nextListElement = listIt.getNextElement();
         TR::Block *block = _blockInfo[nextListElement];
         TR::CFGEdgeList &exceptionSuccs = block->getExceptionSuccessors();

         bool origExceptionSuccs = false;
         if (!exceptionSuccs.empty())
            origExceptionSuccs = true;

         if (!block->getLastRealTreeTop()->getNode()->getOpCode().isReturn())
            {
            if (firstTime)
               {
               addedNode = true;
               cfg->addNode(catchBlock);
               _lastTreeTop->join(catchBlock->getEntry());
               _lastTreeTop = catchBlock->getExit();
               }

            firstTime = false;
            cfg->addExceptionEdge(block, catchBlock);
            if (trace())
               traceMsg(comp(), "Added edge from block_%d to catch block_%d\n", block->getNumber(), catchBlock->getNumber());

            if (origExceptionSuccs)
               {
               for (auto edge = exceptionSuccs.begin(); edge != exceptionSuccs.end();)
                  {
                  if ((*edge)->getTo() != catchBlock)
                     {
                     if (!catchBlock->hasExceptionSuccessor((*edge)->getTo()))
                         cfg->addExceptionEdge(catchBlock, (*edge)->getTo());
                        cfg->removeEdge(*(edge++));
                     }
                  else
                     ++edge;
                  }
               }
            }

         if (addedNode && !catchBlock->hasSuccessor(cfg->getEnd()))
            cfg->addEdge(catchBlock, cfg->getEnd());
         }

      listElem = listElem->getNextElement();
      }
   }




bool
TR::MonitorElimination::addClassThatShouldNotBeLoaded(char *name, int32_t len, TR_LinkHead<TR_ClassLoadCheck> *classesThatShouldNotBeLoaded, bool stackAllocation)
   {
   for (TR_ClassLoadCheck * clc = classesThatShouldNotBeLoaded->getFirst(); clc; clc = clc->getNext())
      if (clc->_length == len && !strncmp(clc->_name, name, len))
         return false;

   if (stackAllocation)
      classesThatShouldNotBeLoaded->add(new (trStackMemory()) TR_ClassLoadCheck(name, len));
   else
      classesThatShouldNotBeLoaded->add(new (trHeapMemory()) TR_ClassLoadCheck(name, len));

   return true;
   }



bool
TR::MonitorElimination::addClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz, TR_LinkHead<TR_ClassExtendCheck> *classesThatShouldNotBeNewlyExtended, bool stackAllocation)
   {
   for (TR_ClassExtendCheck *clc = classesThatShouldNotBeNewlyExtended->getFirst(); clc; clc = clc->getNext())
      if (clc->_clazz == clazz)
         return false;

   if (stackAllocation)
      classesThatShouldNotBeNewlyExtended->add(new (trStackMemory()) TR_ClassExtendCheck(clazz));
   else
      classesThatShouldNotBeNewlyExtended->add(new (trHeapMemory()) TR_ClassExtendCheck(clazz));

   return true;
   }





void TR::MonitorElimination::coarsenSpecialBlock(SpecialBlockInfo *blockInfo)
   {
   _invalidateUseDefInfo = true;
   _invalidateValueNumberInfo = true;
   _invalidateAliasSets = true;

   TR::Block *block = blockInfo->_succBlock;
   int32_t blockNum = block->getNumber();
   TR::TreeTop *treeTop = blockInfo->_treeTop;
   TR::TreeTop *lastTreeTop = comp()->getMethodSymbol()->getLastTreeTop();
   if (!comp()->getClassesThatShouldNotBeLoaded()->isEmpty() ||
        !comp()->getClassesThatShouldNotBeNewlyExtended()->isEmpty())
       {
       requestOpt(OMR::virtualGuardTailSplitter);

       //TR_BlockCloner cloner(comp()->getFlowGraph(), true, true);

       TR::Block *newBlock = blockInfo->_clonedBlock; // cloner.cloneBlocks(block, block);
       lastTreeTop->join(newBlock->getEntry());

       TR::CFG *cfg = comp()->getFlowGraph();
       TR::Block *gotoBlock = NULL;
       if (block->getNextBlock() &&
           block->hasSuccessor(block->getNextBlock()))
          {
          TR::CFGEdge *e = block->getEdge(block->getNextBlock());
          gotoBlock = TR::Block::createEmptyBlock(block->getEntry()->getNode(), comp(), e->getFrequency(), block);
          }

       if (gotoBlock)
          {
          TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(block->getEntry()->getNode(), TR::Goto, 0, block->getNextBlock()->getEntry()));
          gotoBlock->append(gotoTreeTop);
          newBlock->getExit()->join(gotoBlock->getEntry());
          gotoBlock->getExit()->setNextTreeTop(NULL);
          gotoBlock->setIsCold();
          gotoBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
          newBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), block->getNextBlock()->getEntry(), gotoBlock->getEntry());
          lastTreeTop = gotoBlock->getExit();
          }
       else
          lastTreeTop = newBlock->getExit();

       TR::Block *guardBlock = TR::Block::createEmptyBlock(block->getEntry()->getNode(), comp(), block->getFrequency(), block);

       cfg->addNode(guardBlock);
       TR::CFGEdge *newEdge = NULL;
       if (gotoBlock)
          {
          cfg->addNode(gotoBlock);
          newEdge = TR::CFGEdge::createEdge(newBlock,  gotoBlock, trMemory());
          cfg->addEdge(newEdge);
          newEdge = TR::CFGEdge::createEdge(gotoBlock, block->getNextBlock(), trMemory());
          cfg->addEdge(newEdge);
          cfg->removeEdge(newBlock, block->getNextBlock());
          }

       for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
          {
          TR::Block *pred = (*nextEdge)->getFrom()->asBlock();
          if (!pred->hasSuccessor(guardBlock))
             {
             (*nextEdge)->setTo(guardBlock);
             pred->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), block->getEntry(), guardBlock->getEntry());
             //guardBlock->getPredecessors().add(nextEdge);
             }
          }

       block->getPredecessors().clear();

       for (auto nextEdge = block->getExceptionPredecessors().begin(); nextEdge != block->getExceptionPredecessors().end(); ++nextEdge)
          {
          TR::Block *pred = (*nextEdge)->getFrom()->asBlock();
          cfg->addExceptionEdge(pred, guardBlock);
          }
       for (auto nextEdge = block->getExceptionSuccessors().begin(); nextEdge != block->getExceptionSuccessors().end(); ++nextEdge)
          {
          TR::Block *succ = (*nextEdge)->getTo()->asBlock();
          cfg->addExceptionEdge(guardBlock, succ);
          if (gotoBlock)
             cfg->addExceptionEdge(gotoBlock, succ);
          }

       for (auto nextEdge = block->getSuccessors().begin(); nextEdge != block->getSuccessors().end(); ++nextEdge)
          {
          TR::Block *succ = (*nextEdge)->getTo()->asBlock();
          if (gotoBlock &&
              (succ == block->getNextBlock()))
             {
             // Already added above
             //cfg->addEdge(gotoBlock, succ);
             }
          else
             cfg->addEdge(newBlock, succ);
          }

       newEdge = TR::CFGEdge::createEdge(guardBlock,  block, trMemory());
       cfg->addEdge(newEdge);
       newEdge = TR::CFGEdge::createEdge(guardBlock,  newBlock, trMemory());
       cfg->addEdge(newEdge);

       TR::TreeTop *insertionTree = block->getEntry()->getPrevTreeTop();
       if (insertionTree)
          {
          insertionTree->join(guardBlock->getEntry());
          }
       else
          comp()->setStartTree(guardBlock->getEntry());

       guardBlock->getExit()->join(block->getEntry());
       TR::Node *guard = comp()->createSideEffectGuard(comp(),guardBlock->getEntry()->getNode(), newBlock->getEntry());
       TR::TreeTop *guardTree = TR::TreeTop::create(comp(), guard, NULL, NULL);
       guardBlock->append(guardTree);
       }


   TR::TreeTop *monentTree = _monentTrees[blockNum];
   TR::TreeTop *monexitTree = NULL;
   TR::TreeTop *currTree = monentTree->getNextTreeTop();
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      if ((currNode->getOpCodeValue() == TR::monexit) ||
          ((currNode->getNumChildren() > 0) &&
           (currNode->getFirstChild()->getOpCodeValue() == TR::monexit)))
        {
        monexitTree = currTree;
        break;
        }

      if ((currNode->getOpCodeValue() == TR::treetop) &&
          currNode->getFirstChild()->getOpCode().isStore())
        currNode = currNode->getFirstChild();

      //if (!_alreadyClonedNodes.find(currNode))
         {
         TR::Node *newNode = NULL;

         if (currNode->getOpCode().isStore())
            {
            if (!_alreadyClonedRhs.find(currNode->getFirstChild()))
               {
               newNode = currNode->duplicateTree();
               _alreadyClonedNodes.add(newNode);
               }

            if (newNode &&
                !_alreadyClonedNodes.find(currNode))
               {
               TR::SymbolReference *symRef = currNode->getSymbolReference();
               TR::SymbolReference *newSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), symRef->getSymbol()->getDataType());
               newNode->setSymbolReference(newSymRef);

               if (symRef->getSymbol()->isNotCollected())
                  newSymRef->getSymbol()->setNotCollected();

               TR::Node *firstChild = currNode->getFirstChild();
               int32_t j;
               for (j=0;j<firstChild->getNumChildren();j++)
                  firstChild->getChild(j)->recursivelyDecReferenceCount();

               firstChild->setNumChildren(0);
               TR::Node::recreate(firstChild, comp()->il.opCodeForDirectLoad(symRef->getSymbol()->getDataType()));
               firstChild->setSymbolReference(newSymRef);
               _alreadyClonedRhs.add(firstChild);
               _alreadyClonedNodes.add(newNode);
               }
            else if (_alreadyClonedNodes.find(currNode))
               {
               if (currNode->getReferenceCount() > 0)
                  currNode->recursivelyDecReferenceCount();
               else
                  {
                  int32_t j;
                  for (j=0;j<currNode->getNumChildren();j++)
                     currNode->getChild(j)->recursivelyDecReferenceCount();
                  }
               TR::TreeTop *prevTree = currTree->getPrevTreeTop();
               TR::TreeTop *nextTree = currTree->getNextTreeTop();
               prevTree->join(nextTree);
               currTree = prevTree;
               }
            }
         else
            {
            newNode = currNode->duplicateTree();
            _alreadyClonedNodes.add(newNode);
            }

         if (newNode)
            {
            TR::TreeTop *newTree = TR::TreeTop::create(comp(), newNode, NULL, NULL);
            TR::TreeTop *nextTree = treeTop->getNextTreeTop();
            treeTop->join(newTree);
            newTree->join(nextTree);
            treeTop = newTree;
            }
         }
      currTree = currTree->getNextTreeTop();
      }

   TR::TreeTop *prevTree = monentTree->getPrevTreeTop();
   TR::TreeTop *nextTree = monentTree->getNextTreeTop();
   monentTree->getNode()->getFirstChild()->recursivelyDecReferenceCount();
   prevTree->join(nextTree);

   prevTree = monexitTree->getPrevTreeTop();
   nextTree = monexitTree->getNextTreeTop();
   monexitTree->getNode()->getFirstChild()->recursivelyDecReferenceCount();
   prevTree->join(nextTree);

   ListIterator<SpecialBlockInfo> blocksIt(&_specialBlockInfo);
   SpecialBlockInfo *specialBlock;
   for (specialBlock = blocksIt.getFirst(); specialBlock != NULL; specialBlock = blocksIt.getNext())
      {
      if (specialBlock->_treeTop == prevTree)
         specialBlock->_treeTop = treeTop;
      }
   }

bool TR::MonitorElimination::checkIfSuccsInList(TR::CFGEdgeList& edgesList, TR_BitVector *list, bool checkIfControlFlowInCatch, TR::Block *block)
   {
   for (auto edge = edgesList.begin(); edge != edgesList.end(); ++edge)
      {
      int32_t toNum = (*edge)->getTo()->getNumber();
      if (!list->get(toNum))
        {
        if (_guardedVirtualCallBlocks->get(toNum))
          // _adjustedMonexitBlocks->set(toNum);
          {
          _monexitEdges.add(*edge);
          }
        else
           {
           if (block)
              {
              bool result = true;
              if (std::find(block->getExceptionSuccessors().begin(), block->getExceptionSuccessors().end(), *edge) == block->getExceptionSuccessors().end())
                 {
                 if (trace())
                    traceMsg(comp(), "0Tripped on succ %d(%d)\n", (*edge)->getTo()->getNumber(), checkIfControlFlowInCatch);
                 result = false;
                 }

              if (!result &&
                  checkIfControlFlowInCatch)
                 {
                 if (block->getExceptionSuccessors().size() == 1)
                    {
                    TR::CFGNode *catchBlock = block->getExceptionSuccessors().front()->getTo();
                    if (std::find(catchBlock->getSuccessors().begin(), catchBlock->getSuccessors().end(), *edge) != catchBlock->getSuccessors().end())
                       result = true;
                    }
                 }

              if (!result)
                {
                _monexitEdges.add(*edge);
                //return result;
                }
              }
           else
              {
              if (trace())
                 traceMsg(comp(), "1Tripped on succ %d\n", (*edge)->getTo()->getNumber());

              _monexitEdges.add(*edge);
              //return false;
              }
           }
        }
      }
   return true;
   }






//bool TR::MonitorElimination::checkIfPredsInList(TR_PredecessorIterator *edgesIt, TR_BitVector *list, TR::Block *block)
bool TR::MonitorElimination::checkIfPredsInList(TR::CFGEdgeList& edgesIt, TR_BitVector *list, TR::Block *block)
   {
   for (auto edge = edgesIt.begin(); edge != edgesIt.end(); ++edge)
      {
      int32_t fromNum = (*edge)->getFrom()->getNumber();
      if (!list->get(fromNum))
         {
           //if (_guardedVirtualCallBlocks->get(fromNum))
           //_adjustedMonentBlocks->set(fromNum);
           //else
            {
            _monentEdges.add(*edge);
            //return false;
            }
         }
      }
   return true;
   }





TR::CoarsenedMonitorInfo *TR::MonitorElimination::findOrCreateCoarsenedMonitorInfo(int32_t lockedObject, TR::Node *lockedNode)
   {
   TR::CoarsenedMonitorInfo *monitorInfo = findCoarsenedMonitorInfo(lockedObject);
   if (!monitorInfo)
      {
      monitorInfo = new (trStackMemory()) TR::CoarsenedMonitorInfo(trMemory(), lockedObject, comp()->getFlowGraph()->getNextNodeNumber(), lockedNode);
      _coarsenedMonitorsInfo.add(monitorInfo);
      }

   return monitorInfo;
   }



TR::CoarsenedMonitorInfo *TR::MonitorElimination::findCoarsenedMonitorInfo(int32_t lockedObject)
   {
   ListElement<TR::CoarsenedMonitorInfo> *listElem = _coarsenedMonitorsInfo.getListHead();
   while (listElem)
      {
      TR::CoarsenedMonitorInfo *monitorInfo = listElem->getData();
      if (monitorInfo->getMonitorNumber() == lockedObject)
        return monitorInfo;

      listElem = listElem->getNextElement();
      }

   return NULL;
   }




bool TR::MonitorElimination::isSimpleLockedRegion(TR::TreeTop *monentTree)
   {
   _symRefsInSimpleLockedRegion->empty();
   _storedSymRefsInSimpleLockedRegion->empty();
   vcount_t visitCount = comp()->incVisitCount();

   TR::Node *monNode = monentTree->getNode();
   if ((monNode->getOpCodeValue() == TR::monexit) ||
       ((monNode->getNumChildren() > 0) &&
        (monNode->getFirstChild()->getOpCodeValue() == TR::monexit)))
      {
      TR::TreeTop *cursorTree = monentTree->getPrevTreeTop();
      while (cursorTree)
         {
         TR::Node *cursorNode = cursorTree->getNode();
         if ((cursorNode->getOpCodeValue() == TR::monexit) ||
             ((cursorNode->getNumChildren() > 0) &&
              (cursorNode->getFirstChild()->getOpCodeValue() == TR::monexit)))
            return false;

         if ((cursorNode->exceptionsRaised() != 0) ||
             (cursorNode->getOpCode().isStoreIndirect()) ||
             (cursorNode->getOpCode().isStore() &&
              cursorNode->getSymbolReference()->getSymbol()->isStatic()) ||
             (cursorNode->getOpCodeValue() == TR::BBStart))
            return false;

         if ((cursorNode->getOpCodeValue() == TR::monent) ||
             ((cursorNode->getNumChildren() > 0) &&
              (cursorNode->getFirstChild()->getOpCodeValue() == TR::monent)))
            {
            monentTree = cursorTree;
            break;
            }

         cursorTree = cursorTree->getPrevTreeTop();
         }
      }

   TR::TreeTop *currTree = monentTree->getNextTreeTop();
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      if ((currNode->getOpCodeValue() == TR::monexit) ||
          ((currNode->getNumChildren() > 0) &&
           (currNode->getFirstChild()->getOpCodeValue() == TR::monexit)))
        {
        *_scratchForSymbols = *_symRefsInSimpleLockedRegion;
        *_scratchForSymbols &= *_storedSymRefsInSimpleLockedRegion;
        if (_scratchForSymbols->isEmpty())
           return true;
        else
           return false;
        }

      if ((currNode->getOpCodeValue() == TR::monent) ||
          (currNode->exceptionsRaised() != 0) ||
          (currNode->getOpCode().isStoreIndirect()) ||
          (currNode->getOpCodeValue() == TR::BBEnd))
        {
        return false;
        }

      collectSymRefsInSimpleLockedRegion(currNode, visitCount);
      currTree = currTree->getNextTreeTop();
      }

   return false;
   }




void TR::MonitorElimination::collectSymRefsInSimpleLockedRegion(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadVar())
      _symRefsInSimpleLockedRegion->set(node->getSymbolReference()->getReferenceNumber());

   if (node->getOpCode().isStore())
     _storedSymRefsInSimpleLockedRegion->set(node->getSymbolReference()->getReferenceNumber());

   int32_t i;
   for (i=0;i<node->getNumChildren();i++)
      collectSymRefsInSimpleLockedRegion(node->getChild(i), visitCount);
   }



bool TR::MonitorElimination::callsAllowCoarsening()
   {
   bool recoveryPossible = true;

   TR_BitVectorIterator bvi(*_scratch);
   while (bvi.hasMoreElements())
      {
      int32_t nextCallBlockNum = bvi.getNextElement();
      TR::Block *nextCallBlock = _blockInfo[nextCallBlockNum];
      TR::TreeTop *entryTree = nextCallBlock->getEntry();
      TR::TreeTop *exitTree = nextCallBlock->getExit();
      bool peekedCall = false;
      recoveryPossible = treesAllowCoarsening(entryTree, exitTree, &peekedCall);

      if (!recoveryPossible)
        break;
      }

   return recoveryPossible;
   }


bool TR::MonitorElimination::treesAllowCoarsening(TR::TreeTop *startTree, TR::TreeTop *endTree, bool *peekedCall, bool *seenCheck)
   {
   TR::TreeTop *cursorTree = startTree;
   bool recoveryPossible = true;

   if (seenCheck)
      *seenCheck = false;

   while (cursorTree != endTree)
      {
      TR::Node *node = cursorTree->getNode();
      if (node->getOpCode().isResolveCheck() ||
          ((node->getOpCodeValue() == TR::monexit) ||
           (node->getOpCodeValue() == TR::monent)) ||
          ((node->getNumChildren() > 0) &&
           ((node->getFirstChild()->getOpCodeValue() == TR::monexit) ||
            (node->getFirstChild()->getOpCodeValue() == TR::monent))))
         {
         recoveryPossible = false;
         break;
         }

      if (node->exceptionsRaised() != 0)
         {
         if (seenCheck)
            *seenCheck = true;
         }

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCodeValue() == TR::NULLCHK)
         node = node->getFirstChild();

      if (node->getOpCode().isCall() &&
          node->getSymbolReference()->isUnresolved())
         {
         recoveryPossible = false;
         break;
         }

      if (node->getOpCode().isCall() &&
          //node->getOpCode().isIndirect() &&
          !node->getSymbolReference()->isUnresolved() &&
          !node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper())
         {
         TR_ResolvedMethod *resolvedMethod = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getResolvedMethodSymbol()->getResolvedMethod();

         TR::CoarseningInterProceduralAnalyzer ipaAnalyzer(comp(), trace());
         //dumpOptDetails(comp(), "Analyzing call %p to method %s\n", node, resolvedMethod->signature());
         if (!ipaAnalyzer.analyzeCall(node))
            {
            recoveryPossible = false;
            if (trace())
               traceMsg(comp(), "Recovery is NOT possible from call %p to method %s\n", node, resolvedMethod->signature(trMemory()));
            break;
            }
         else
            {
            *peekedCall = true;
            for (TR::GlobalSymbol *currSym = ipaAnalyzer._globalsWritten.getFirst(); currSym; currSym = currSym->getNext())
               {
               TR::SymbolReference *currSymReference = currSym->_symRef;
               TR_BitVectorIterator bvi(*_symRefsInSimpleLockedRegion);
               TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
               while (bvi.hasMoreElements())
                  {
                  int32_t symRefNum = bvi.getNextElement();
                  TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNum);
                  if (symRef->getSymbol()->isStatic() ||
                      (symRef->getSymbol()->isShadow() &&
                       !symRef->getSymbol()->isArrayShadowSymbol()))
                     {
                     char *sig = NULL;
                     int32_t length = 0;
                     if (symRef->getSymbol()->isStatic())
                        {
                        if (symRef->getSymbol()->isConstObjectRef())
                           continue; // not a static field -- no danger
                        else
                           sig = symRef->getOwningMethod(comp())->staticName(symRef->getCPIndex(), length, trMemory());
                        }
                     else if (symRef->getSymbol()->isShadow())
                        {
                        sig = symRef->getOwningMethod(comp())->fieldName(symRef->getCPIndex(), length, trMemory());
                        }

                     char *currSig = NULL;
                     int32_t currLength = 0;
                     if (currSymReference->getSymbol()->isStatic())
                        {
                        if (currSymReference->getSymbol()->isConstObjectRef())
                           continue; // not a static field -- no danger
                        else
                           currSig = currSymReference->getOwningMethod(comp())->staticName(currSymReference->getCPIndex(), currLength, trMemory());
                        }
                     else if (currSymReference->getSymbol()->isShadow())
                        {
                        currSig = currSymReference->getOwningMethod(comp())->fieldName(currSymReference->getCPIndex(), currLength, trMemory());
                        }

                     if ((length == currLength) &&
                         (memcmp(sig, currSig, length) == 0))
                        {
                        if (trace())
                           traceMsg(comp(), "Recovery is NOT possible from call %p to method %s due to written symbols\n", node, resolvedMethod->signature(trMemory()));
                        return false;
                        }
                     }
                  }
               }

            for (TR_ClassLoadCheck * clc = ipaAnalyzer._classesThatShouldNotBeLoaded.getFirst(); clc; clc = clc->getNext())
               addClassThatShouldNotBeLoaded(clc->_name, clc->_length, &_classesThatShouldNotBeLoaded, true);

            for (TR_ClassExtendCheck * cec = ipaAnalyzer._classesThatShouldNotBeNewlyExtended.getFirst(); cec; cec = cec->getNext())
               addClassThatShouldNotBeNewlyExtended(cec->_clazz, &_classesThatShouldNotBeNewlyExtended, true);
            }
         }
      cursorTree = cursorTree->getNextTreeTop();
      }

   return recoveryPossible;
   }



bool TR::MonitorElimination::symbolsAreNotWritten(TR_BitVector *intersection)
   {
   bool symbolsNotWritten = true;

   TR_BitVectorIterator bvi(*intersection);
   while (bvi.hasMoreElements())
      {
      int32_t nextBlockNum = bvi.getNextElement();
      TR::Block *nextBlock = _blockInfo[nextBlockNum];
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();
      *_writtenSymbols = *(_symbolsWritten[nextBlockNum]);
      *_writtenSymbols &= *_symRefsInSimpleLockedRegion;
      if (!_writtenSymbols->isEmpty())
         symbolsNotWritten = false;
      //symbolsNotWritten = symbolsAreNotWrittenInTrees(entryTree, exitTree);

      if (!symbolsNotWritten)
        break;
      }

   return symbolsNotWritten;
   }



bool TR::MonitorElimination::symbolsAreNotWrittenInTrees(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   TR::TreeTop *cursorTree = startTree;
   bool symbolsNotWritten = true;

   while (cursorTree != endTree)
      {
      TR::Node *cursorNode = cursorTree->getNode();

      if (cursorNode->getOpCode().isNullCheck() ||
          cursorNode->getOpCode().isResolveCheck() ||
          (cursorNode->getOpCodeValue() == TR::treetop))
         cursorNode = cursorNode->getFirstChild();

      if (cursorNode->getOpCode().isStore() ||
          cursorNode->mightHaveVolatileSymbolReference())
         {
         TR::SymbolReference *symReference = cursorNode->getSymbolReference();
         if (_symRefsInSimpleLockedRegion->get(symReference->getReferenceNumber()))
            symbolsNotWritten = false;
         if (symReference->sharesSymbol())
            {
            if (symReference->getUseDefAliases().containsAny(*_symRefsInSimpleLockedRegion, comp()) )
               symbolsNotWritten = false;
            }
         }
      else if (/* cursorNode->getOpCode().isCall() || */
               (cursorNode->isGCSafePointWithSymRef()) && comp()->getOptions()->realTimeGC() ||
               (cursorNode->getOpCode().hasSymbolReference() &&
                cursorNode->getSymbolReference()->isUnresolved()))
         {
         bool isCallDirect = false;
         if (cursorNode->getOpCode().isCallDirect())
            isCallDirect = true;

         if (cursorNode->getSymbolReference()->getUseDefAliases(isCallDirect).containsAny(*_symRefsInSimpleLockedRegion, comp()))
            symbolsNotWritten = false;
         }

      if (!symbolsNotWritten)
         break;

      cursorTree = cursorTree->getNextTreeTop();
      }

   return symbolsNotWritten;
   }






bool TR::CoarseningInterProceduralAnalyzer::analyzeNode(TR::Node *node, vcount_t visitCount, bool *success)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   TR::SymbolReference *symRef = NULL;
   if (node->getOpCode().hasSymbolReference())
      symRef = node->getSymbolReference();

   if (symRef &&
       symRef->isUnresolved())
       {
       if (!(symRef->getSymbol()->isStatic() &&
             symRef->getSymbol()->castToStaticSymbol()->isConstObjectRef()))
          {
          if ((node->getOpCodeValue() == TR::loadaddr) &&
              (node->getSymbolReference()->getSymbol()->isClassObject()))
             {
             uint32_t classNameLength;
             char *className = symRef->getOwningMethod(comp())->getClassNameFromConstantPool(symRef->getCPIndex(), classNameLength);

             if (!className)
                {
                *success = false;
                if (trace())
                   {
                   traceMsg(comp(), "Found unresolved class object load %p while peeking and unable to add assumption -- peek unsuccessful\n", node);
                   //printf("Found unresolved class object load %p while peeking and unable to add assumption -- peek unsuccessful\n", node);
                   }
                }
             else
                {
                addClassThatShouldNotBeLoaded(className, classNameLength);
                if (trace())
                   {
                   traceMsg(comp(), "Found unresolved class object node %p while peeking -- add assumption -- skip peeking in rest of block\n", node);
                   //printf("Found unresolved class object node %p while peeking -- add assumption for class %s\n", node, className);
                   }

                // Add assumption here
                //
                return true;
                }
             }
          else
             {
             char *className = NULL;
             int32_t classnameLength = -1;

             if (symRef->getSymbol()->isShadow() ||
                 symRef->getSymbol()->isStatic())
                {
                className = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), classnameLength);
                }
             else if (symRef->getSymbol()->getMethodSymbol())
                {
                TR::MethodSymbol *methodSymbol = symRef->getSymbol()->getMethodSymbol();
                TR_Method * originalMethod = methodSymbol->getMethod();
                if (originalMethod)
                   {
                   classnameLength = originalMethod->classNameLength();
                   className = classNameToSignature(originalMethod->classNameChars(), classnameLength, comp());
                   }
                }

             if (className)
                {
                TR_OpaqueClassBlock *declaringClazz = comp()->fej9()->getClassFromSignature(className, classnameLength, symRef->getOwningMethod(comp()));
                if (!declaringClazz)
                   {
                   addClassThatShouldNotBeLoaded(className, classnameLength);

                   if (trace())
                      {
                      traceMsg(comp(), "Found unresolved class object node %p while peeking -- skip peeking in rest of block\n", node);
                      //printf("Found unresolved class object node %p while peeking -- add assumption for class %s\n", node, className);
                      }

                   return true;
                   }
                /*
                  // Peek should not suffer if ONLY the field is unresolved and class is resolved
                  // as class loading or any other user code cannot run at this point; this assertion may
                  // be invalid; so check VM spec
                  //
                else
                   {
                   *success = false;
                   if (trace())
                      {
                      traceMsg(comp(), "Found unresolved node %p while peeking whose class in resolved -- peek unsuccessful\n", node);
                      printf("Found unresolved node %p while peeking whose class %s in resolved -- peek unsuccessful\n", node, className);
                      }
                   }
                */
                }
             else
                {
                *success = false;
                if (trace())
                   {
                   traceMsg(comp(), "Found unresolved node %p while peeking whose class is unresolved and unable to add assumption -- peek unsuccessful\n", node);
                   //printf("Found unresolved node %p while peeking whose class is unresolved and unable to add assumption -- peek unsuccessful\n", node);
                   }
                }
             }

          return false;
          }
       }

   if (node->getOpCode().isStore())
      {
      if (node->getSymbolReference()->getSymbol()->isStatic() ||
          (node->getSymbolReference()->getSymbol()->isShadow() &&
           !node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()))
         addWrittenGlobal(node->getSymbolReference());
      }

   if (node->getOpCodeValue() == TR::monent ||
       node->getOpCodeValue() == TR::monexit)
      {
      if (trace())
        {
        //printf("Found monitor node %p while peeking -- peek unsuccessful\n", node);
        traceMsg(comp(), "Found monitor node %p while peeking -- peek unsuccessful\n", node);
        }
      *success = false;
      return false;
      }

   int32_t i;
   for (i=0;i<node->getNumChildren();i++)
      {
      if (analyzeNode(node->getChild(i), visitCount, success))
         return true;
      }

   return false;
   }
