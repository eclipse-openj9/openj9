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

#ifndef MONITORELIM_INCL
#define MONITORELIM_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/InterProceduralAnalyzer.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "ras/LogTracer.hpp"

class TR_ActiveMonitor;
class TR_ClassExtendCheck;
class TR_ClassLoadCheck;
class TR_OpaqueClassBlock;
class TR_Structure;
class TR_StructureSubGraphNode;
namespace TR { class SymbolReference; }
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class Compilation; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;
template <class T> class TR_Stack;

// Monitor elimination
//
// Find monitors that are nested within other monitors on the same object
// and eliminate them.
//
// Requires value numbering to identify objects being monitored.
//

/*class TR_TransactionalRegionTransformer
   {
   public:
   TR_Alloc(TR_Memory::MonitorElimination)      //change this later

   TR_TransactionalRegionTransformer

   private:
*/

namespace TR {

class CoarseningInterProceduralAnalyzer : public TR::InterProceduralAnalyzer
   {
   public:
   TR_ALLOC(TR_Memory::CoarseningInterProceduralAnalyzer)

   CoarseningInterProceduralAnalyzer(TR::Compilation *c, bool trace)
      : TR::InterProceduralAnalyzer(c, trace)
     {
     }

   virtual bool analyzeNode(TR::Node *, vcount_t, bool *);
   };

struct SpecialBlockInfo
   {
   TR::Block *_succBlock;
     //TR::Block *_predBlock;
   TR::TreeTop *_treeTop;
   TR::Block *_clonedBlock;
   };

class CoarsenedMonitorInfo
   {
   public:
   TR_ALLOC(TR_Memory::MonitorElimination)

   CoarsenedMonitorInfo(TR_Memory * m, int32_t monitorNumber, int32_t numBlocks, TR::Node *monitorNode = NULL)
     : _monitorNumber(monitorNumber),
       _monentBlocks(numBlocks, m, stackAlloc),
       _monexitBlocks(numBlocks, m, stackAlloc),
       _interveningBlocks(numBlocks, m, stackAlloc),
       _monentEdges(m),
       _monexitEdges(m),
       _monitorNode(monitorNode)
      {
      }

   int32_t getMonitorNumber() {return _monitorNumber;}
   TR::Node *getMonitorNode() {return _monitorNode;}
   bool adjustedMonentBlock(int32_t blockNum) {if (_monentBlocks.get(blockNum)) return true; return false;}
   bool adjustedMonexitBlock(int32_t blockNum) {if (_monexitBlocks.get(blockNum)) return true; return false;}

   void setAdjustedMonentBlock(int32_t blockNum) {_monentBlocks.set(blockNum);}
   void setAdjustedMonexitBlock(int32_t blockNum) {_monexitBlocks.set(blockNum);}

   List<TR::CFGEdge> &getMonentEdges() {return _monentEdges;}
   void addMonentEdge(TR::CFGEdge *edge) {if (!_monentEdges.find(edge)) _monentEdges.add(edge);}

   List<TR::CFGEdge> &getMonexitEdges() {return _monexitEdges;}
   void addMonexitEdge(TR::CFGEdge *edge) {if (!_monexitEdges.find(edge)) _monexitEdges.add(edge);}

   TR_BitVector &getInterveningBlocks() {return _interveningBlocks;}
   void setInterveningBlocks(TR_BitVector *blocks) {_interveningBlocks |= *blocks;}

   private:

   TR::Node *_monitorNode;
   TR_BitVector _monentBlocks;
   TR_BitVector _monexitBlocks;
   TR_BitVector _interveningBlocks;
   List<TR::CFGEdge> _monentEdges;
   List<TR::CFGEdge> _monexitEdges;
   int32_t _monitorNumber;
   };


class MonitorElimination : public TR::Optimization
   {

   bool _hasTMOpportunities;

   public:
   MonitorElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR::MonitorElimination(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   TR_LogTracer* tracer() { return &_tracer; }

   static void collectPredsAndSuccs(TR::CFGNode *, TR_BitVector *, TR_BitVector **, TR_BitVector **, List<TR::CFGEdge> *, TR_BitVector *, TR::Compilation *);

   bool firstPass() { return (manager()->numPassesCompleted() == 0); }

   private :

   bool findRedundantMonitors();
   bool addPath(TR_ActiveMonitor *monitor, TR::Block *block);
   bool addPathAfterSkippingIfNecessary(TR_ActiveMonitor *monitor, TR::Block *block);
   bool addPaths(TR_ActiveMonitor *monitor, TR::CFGEdgeList &paths);
   bool addExceptionPaths(TR_ActiveMonitor *monitor, TR::CFGEdgeList &paths, uint32_t exceptionsRaised);
   void checkRedundantMonitor();
   void removeRedundantMonitors();

   //TransactionalMemory
#ifndef PUBLIC_BUILD
   bool evaluateMonitorsForTMCandidates();
   void transformMonitorsIntoTMRegions();
   void searchAndLabelNearbyMonitors(TR_ActiveMonitor *currentMonitor);
   bool searchDownForOtherMonitorsInCurrentBlock(TR::TreeTop *tt,int32_t &size, TR::TreeTop *& monitorTT);
   bool searchDownForOtherMonitorsInSuccessors(TR::TreeTop *tt,int32_t &size, TR::list<TR::TreeTop *> &closeMonitors, int32_t maxDepth, int32_t minNumberOfNodes);
   TR_ActiveMonitor * findActiveMonitor(TR::TreeTop *tt);
   bool hasMultipleEntriesWithSameExit(TR_ActiveMonitor *monitor);

   TR_Array<TR::Block *>* createFailHandlerBlocks(TR_ActiveMonitor *monitor, TR::SymbolReference *tempSymRef, TR::Block *monitorBlock, TR::Block *tstartblock);
   TR::SymbolReference *createAndInsertTMRetryCounter(TR_ActiveMonitor *monitor);
#endif
   void resetReadMonitors(int32_t);
   bool tagReadMonitors();
   bool transformIntoReadMonitor();
   void recognizeIfThenReadRegion(TR::TreeTop *, TR::Node *, int32_t, TR::Block *, TR::Block *);
   bool killsReadMonitorProperty(TR::Node *);
   bool preservesReadRegion(TR::Node *, TR::Block *, TR::Node **);
   TR::Block *adjustBlockToCreateReadMonitor(TR::TreeTop *, TR::Node *, int32_t, TR::Block *, TR::Block *, TR::Block *);

   void coarsenMonitorRanges();
   void coarsenMonitor(int32_t, int32_t, TR::Node *);
   void removeMonitorNode(TR::Node *node);

   void rematMonitorEntry(TR_ActiveMonitor *monitor);
   void addOSRGuard(TR::TreeTop *guard);

   bool markBlocksAtSameNestingLevel(TR_Structure *, TR_BitVector *);
   TR_BitVector *getBlocksAtSameNestingLevel(TR::Block *);
   void collectCFGBackEdges(TR_StructureSubGraphNode *);
     //bool checkIfSuccsInList(TR_SuccessorIterator *, TR_BitVector *, bool checkIfControlFlowInCatch = false, TR::Block *block = NULL);
     //bool checkIfPredsInList(TR_PredecessorIterator *, TR_BitVector *, TR::Block *block = NULL);
   bool checkIfSuccsInList(TR::CFGEdgeList&, TR_BitVector *, bool checkIfControlFlowInCatch = false, TR::Block *block = NULL);
   bool checkIfPredsInList(TR::CFGEdgeList&, TR_BitVector *, TR::Block *block = NULL);

   void adjustMonentAndMonexitBlocks(TR::Node *, TR_BitVector *, int32_t);
   void prependMonexitAndAppendMonentInBlock(TR::Node *, TR::Block *, int32_t);
   void adjustMonexitBlocks(TR::Node *, int32_t);
   void prependMonexitInBlock(TR::Node *, TR::Block *, int32_t, bool insertNullTest = true);
   void prependMonexitInBlock(TR::Node *, TR::Block *, bool insertNullTest = true);
   void adjustMonentBlocks(TR::Node *, int32_t);
   void appendMonentInBlock(TR::Node *, TR::Block *, int32_t, bool insertNullTest = true);
   void appendMonentInBlock(TR::Node *, TR::Block *, bool insertNullTest = true);
   void removeLastMonexitInBlock(TR::Block *);
   void removeFirstMonentInBlock(TR::Block *);
   void insertNullTestBeforeBlock(TR::Node *, TR::Block *);

   TR::Block *findOrSplitEdge(TR::Block *, TR::Block *);
   void splitEdgesAndAddMonitors();
   void addCatchBlocks();

   void buildClosure(int32_t, TR_BitVector *, TR_BitVector *, int32_t);
   void collectSuccessors(int32_t, TR_BitVector *, TR_BitVector *, int32_t);
   void collectPredecessors(int32_t, TR_BitVector *, TR_BitVector *, int32_t);

   TR::CoarsenedMonitorInfo *findOrCreateCoarsenedMonitorInfo(int32_t, TR::Node *);
   TR::CoarsenedMonitorInfo *findCoarsenedMonitorInfo(int32_t);

   bool isSimpleLockedRegion(TR::TreeTop *);
   void collectSymRefsInSimpleLockedRegion(TR::Node *, vcount_t);
   bool callsAllowCoarsening();
   bool treesAllowCoarsening(TR::TreeTop *, TR::TreeTop *, bool *, bool *seenCheck = NULL);
   bool symbolsAreNotWritten(TR_BitVector *);
   bool symbolsAreNotWrittenInTrees(TR::TreeTop *, TR::TreeTop *);
   void coarsenSpecialBlock(SpecialBlockInfo *blockInfo);

   bool addClassThatShouldNotBeLoaded(char *name, int32_t len, TR_LinkHead<TR_ClassLoadCheck> *, bool stackAllocation);
   bool addClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz, TR_LinkHead<TR_ClassExtendCheck> *, bool stackAllocation);

   TR_Stack<TR_ActiveMonitor*> *_monitorStack;
   TR_BitVector **_successorInfo;
   TR_BitVector **_predecessorInfo;
   TR_BitVector **_predecessors; // used by single threaded opts
   TR_BitVector *_mightCreateNewThread; // used by single threaded opts
   TR_BitVector *_intersection, *_subtraction, *_temp, *_inclusiveIntersection, *_closureIntersection, *_scratch, *_monexitWorkList, *_loopEntryBlocks;
   TR_BitVector *_guardedVirtualCallBlocks, *_adjustedMonexitBlocks, *_adjustedMonentBlocks, *_matchingMonentBlocks, *_matchingMonexitBlocks, *_matchingSpecialBlocks, *_matchingNormalMonentBlocks, *_matchingNormalMonexitBlocks;
   TR_BitVector *_containsExceptions, *_containsCalls, *_containsAsyncCheck, *_containsMonents, *_containsMonexits, *_lockedObjects, *_multiplyLockedObjects, *_coarsenedMonexits, *_coarsenedMonitors;
   int32_t *_monentBlockInfo;
   int32_t *_monexitBlockInfo;
   int32_t *_safeValueNumbers;
   TR::Block **_blockInfo;
   TR_BitVector **_symbolsWritten;
   TR_BitVector *_symRefsInSimpleLockedRegion, *_storedSymRefsInSimpleLockedRegion, *_scratchForSymbols;
   TR_BitVector *_writtenSymbols;
   TR::TreeTop *_lastTreeTop;
   TR::TreeTop **_monentTrees;
   TR::TreeTop **_monexitTrees;
   TR_ScratchList<TR::Node> _alreadyClonedNodes, _alreadyClonedRhs;

   TR_ScratchList<SpecialBlockInfo> _specialBlockInfo;
   TR_LinkHead<TR_ClassLoadCheck> _classesThatShouldNotBeLoaded;
   TR_LinkHead<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtended;

   TR_ScratchList<TR_ActiveMonitor> _monitors;
   //TR_ScratchList<TR_ActiveMonitor> _tmCandidates;
   List<TR::CoarsenedMonitorInfo> _coarsenedMonitorsInfo;
   List<TR::CFGEdge> _monentEdges, _monexitEdges, _cfgBackEdges;
   List<TR::CFGNode> _splitBlocks;
   List<TR::CFGNode> _nullTestBlocks;

   int32_t _numBlocks;

   bool _invalidateUseDefInfo, _invalidateValueNumberInfo, _invalidateAliasSets;


   TR_LogTracer _tracer;

   };

}

#endif
