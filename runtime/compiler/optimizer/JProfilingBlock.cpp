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
#include "JProfilingBlock.hpp"

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
#include "optimizer/TransformUtil.hpp"

// Global thresholds for the number of method enters required to trip
// method recompilation - these are adjusted in the JIT hook control logic
int32_t TR_JProfilingBlock::nestedLoopRecompileThreshold = 10;
int32_t TR_JProfilingBlock::loopRecompileThreshold = 250;
int32_t TR_JProfilingBlock::recompileThreshold = 500;

/**
 * Prim's algorithm to compute a Minimum Spanning Tree traverses the edges of the tree
 * without regard to direction of the edges. Further for the purposes of placing counters
 * We need to add a special loopback edge connecting the CFG entry and exit nodes.
 * This class provides an edge iterator for a CFG node which presents all successors and
 * predecessors including those from the special loopback edge.
 */
class AdjacentBlockIterator
   {
   TR::Compilation *_comp;
   TR::Block *_block;
   TR::Block *_current;
   int32_t _frequency;
  
   TR::CFGEdgeList::iterator _iterators[8]; 
   TR::BlockChecklist _visitedBlocks;
  
   /**
    * Advance the iterator to the next node, if any 
    */
   void advance()
      {
      TR::CFG *cfg = _comp->getFlowGraph();
      for (int32_t i = 0; i < 8; i += 2)
         {
         if (_iterators[i] != _iterators[i + 1])
            {
            TR::CFGEdge *edge = *(_iterators[i]);
            _current = edge->getTo() == _block ? edge->getFrom()->asBlock() : edge->getTo()->asBlock();
            _frequency = edge->getFrequency();
            _visitedBlocks.add(_current);
            _iterators[i]++;
            return;
            }
         }
      // handle the loopback edge from start to end with an artificially high frequency
      // to make sure the edge is included in the computed MST
      if (cfg->getStart()->asBlock() == _block
          && !_visitedBlocks.contains(cfg->getEnd()->asBlock()))
         {
         _current = cfg->getEnd()->asBlock();
         _frequency = 100000;
         _visitedBlocks.add(_current);
         return;
         }
      // handle the loopback edge from end to start with an artificially high frequency
      // to make sure the edge is included in the computed MST
      if (cfg->getEnd()->asBlock() == _block
          && !_visitedBlocks.contains(cfg->getStart()->asBlock()))
         {
         _current = cfg->getStart()->asBlock();
         _frequency = 100000;
         _visitedBlocks.add(_current);
         return;
         }
      _current = NULL;
      _frequency = -1;
      }
 
   public:
      AdjacentBlockIterator(TR::Compilation *comp, TR::Block *block) :
         _comp(comp),
         _block(block),
         _current(NULL),
         _frequency(-1),
         _visitedBlocks(comp)
         {
         _visitedBlocks.add(block);
         _iterators[0] = block->getPredecessors().begin();
         _iterators[1] = block->getPredecessors().end();
         _iterators[2] = block->getExceptionPredecessors().begin();
         _iterators[3] = block->getExceptionPredecessors().end();
         _iterators[4] = block->getSuccessors().begin();
         _iterators[5] = block->getSuccessors().end();
         _iterators[6] = block->getExceptionSuccessors().begin();
         _iterators[7] = block->getExceptionSuccessors().end();
         advance();
         }

      TR::Block *current() { return _current; }
      int32_t frequency() { return _frequency; }

      void operator ++()
         {
         advance();
         }
   };

/**
 * A wrapper for the AllBlockIterator which also returns the special CFG
 * start and end nodes
 */
class CFGNodeIterator
   {
   private:
   TR::AllBlockIterator _itr;
   TR::CFG *_cfg;
   bool _start;
   bool _end;

   public:
   CFGNodeIterator(TR::CFG *cfg, TR::Optimization *opt) : _cfg(cfg), _itr(cfg, opt->comp()), _start(false), _end(false) { }
   TR::Block *currentBlock()
      {
      TR::Block *itrCurrent = _itr.currentBlock();
      if (itrCurrent)
         return itrCurrent;
      if (!_start)
         return _cfg->getStart()->asBlock();
      if (!_end)
         return _cfg->getEnd()->asBlock();
      return NULL;
      }
   void operator ++()
      {
      if (_itr.currentBlock())
         ++_itr;
      else if (!_start)
         _start = true;
      else if (!_end)
         _end = true;
      }
   };

/**
 * A wrapper for the CFG SuccessorIterator which wraps the normal SuccessorIterator
 * which also returns the special loop back edge added to the tree for the purposes
 * of Minimum Spanning Tree construction
 */
class CFGSuccessorIterator
   {
   private:
   TR::CFG *_cfg;
   TR::CFGEdge *_loopBack;
   TR_SuccessorIterator _itr;
   bool _done;
   TR::Block *_block;
   public:
   CFGSuccessorIterator(TR::CFG *cfg, TR::CFGEdge *loopBack, TR::Block *block)
      : _cfg(cfg), _loopBack(loopBack), _itr(block),
        _done(block != cfg->getEnd()->asBlock()), _block(block)
      {
      }
   TR::CFGEdge *getFirst()
      {
      _itr.getFirst();
      _done = (_block != _cfg->getEnd()->asBlock());
      return getCurrent();
      }
   TR::CFGEdge *getCurrent()
      {
      if (_itr.getCurrent())
         return _itr.getCurrent();
      if (!_done)
         return _loopBack;
      return NULL;
      }
   TR::CFGEdge *getNext()
      {
      bool ended = _itr.getCurrent() == NULL;
      if (!ended)
         _itr.getNext();
      else if (!_done)
         _done = true;
      return getCurrent();
      }
   };

/**
 * A wrapper for the CFG PredecessorIterator which wraps the normal PredecessorIterator
 * which also returns the special loop back edge added to the tree for the purposes
 * of Minimum Spanning Tree construction
 */
class CFGPredecessorIterator
   {
   private:
   TR::CFG *_cfg;
   TR::CFGEdge *_loopBack;
   TR_PredecessorIterator _itr;
   bool _done;
   TR::Block *_block;
   public:
   CFGPredecessorIterator(TR::CFG *cfg, TR::CFGEdge *loopBack, TR::Block *block)
      : _cfg(cfg), _loopBack(loopBack), _itr(block), 
        _done(block != cfg->getStart()->asBlock()), _block(block)
      {
      }
   TR::CFGEdge *getFirst()
      {
      _itr.getFirst();
      _done = (_block != _cfg->getStart()->asBlock());
      return getCurrent();
      }
   TR::CFGEdge *getCurrent()
      {
      if (_itr.getCurrent())
         return _itr.getCurrent();
      if (!_done)
         return _loopBack;
      return NULL;
      }
   TR::CFGEdge *getNext()
      {
      bool ended = _itr.getCurrent() == NULL;
      if (!ended)
         _itr.getNext();
      else if (!_done)
         _done = true;
      return getCurrent();
      }
   };

/**
 * A simple internal data structure used as part of Prim's algorithm to track
 * the current weight for each node in the CFG as part of running the algorithm
 */
class BlockWeights
   {
   int32_t *_weights;
   public:
   BlockWeights(int32_t numBlocks, TR::Region &region)
      {
      _weights = (int32_t*)region.allocate(sizeof(int32_t) * numBlocks);
      for (int32_t i = 0; i < numBlocks; ++i)
         _weights[i] = 1;
      }
   int32_t getWeight(TR::Block *block) { return _weights[block->getNumber()]; }
   void setWeight(TR::Block *block, int32_t weight) { _weights[block->getNumber()] = weight; }
   int32_t &operator [](TR::Block *block) { return _weights[block->getNumber()]; }
   };

/**
 * A data structure used to store a Minimum Spanning Tree (MST) as parent-child
 * key value pairs
 */
class BlockParents
   {
   TR::Block **_parents;
   int32_t _length;
   public:
   BlockParents(int32_t numBlocks, TR::Region &region)
      {
      _parents = (TR::Block **)region.allocate(sizeof(TR::Block*) * numBlocks);
      _length = numBlocks;
      memset(_parents, 0, sizeof(TR::Block *) * numBlocks);
      }
   TR::Block *&operator [](TR::Block *block) { return _parents[block->getNumber()]; }
   void dump(TR::Compilation *comp)
      {
      for (int32_t i = 0; i < _length; ++i)
         {
         if (_parents[i] != NULL)
            traceMsg(comp, "MST edge block_%d to block_%d\n", _parents[i]->getNumber(), i);
         }
      }
   };

/**
 * A data structure which is used to store the sequence of counters to be
 * added and subtracted to compute the frequency of each edge in the CFG.
 * While adding edges the data structure will also compute the counters
 * to be added and subtracted to produce the block frequencies.
 */
class EdgeFrequencyInfo
   {
   TR::Compilation *comp;
   TR::Region &region;
   TR_BitVector **edgeMap;
   TR::CFGEdge *loopBack;
   bool trace;
   void printEdge(TR_BitVector *toAdd, TR_BitVector *toSub)
      {
      if (!toAdd->isEmpty())
         {
         TR_BitVectorIterator bvi(*toAdd);
         while (bvi.hasMoreElements())
            {
            traceMsg(comp, "%d ", bvi.getNextElement());
            }
         }
      else
         {
         traceMsg(comp, "none");
         }
      traceMsg(comp,"\n sub:");
      if (!toSub->isEmpty())
         {
         TR_BitVectorIterator bvi(*toSub);
         while (bvi.hasMoreElements())
            {
            traceMsg(comp, "%d ", bvi.getNextElement());
            }
         }
      else
         {
         traceMsg(comp, "none");
         }
      traceMsg(comp, "\n");
      }
   public:
   EdgeFrequencyInfo(TR::Compilation *comp, TR::CFGEdge *loopBack, int32_t numEdges, TR::Region &region, bool trace = false) : comp(comp), loopBack(loopBack), region(region), trace(trace)
      {
      edgeMap = (TR_BitVector **)region.allocate(sizeof(TR_BitVector *) * numEdges * 2);
      memset(edgeMap, 0, sizeof(TR_BitVector *) * numEdges * 2);
      }

   bool hasEdge(TR::CFGEdge *edge) { TR_ASSERT(edge->getId() > -1, "Edge id must have been set - from block_%d to block_%d!\n", edge->getFrom()->getNumber(), edge->getTo()->getNumber()); return edgeMap[edge->getId() * 2] != NULL; }

   void addAbsoluteEdge(TR::CFGEdge *edge, TR::Block *block)
      {
      if (hasEdge(edge))
         return;

      TR_BitVector *toAdd = new (comp->trStackMemory()) TR_BitVector(5, comp->trMemory(), stackAlloc);
      TR_BitVector *toSub = new (comp->trStackMemory()) TR_BitVector(5, comp->trMemory(), stackAlloc);
      edgeMap[edge->getId() * 2] = toAdd;
      edgeMap[edge->getId() * 2 + 1] = toSub;
      toAdd->set(block->getNumber());

      if (trace)
         {
         traceMsg(comp, "abs edge %d-->%d:\n", edge->getFrom()->asBlock()->getNumber(), edge->getTo()->asBlock()->getNumber());
         printEdge(toAdd, toSub);
         }
      }

   void addEdge(TR::CFGEdge *edge, TR::Block *block)
      {
      if (hasEdge(edge))
         return;

      TR_BitVector *toAdd = new (comp->trStackMemory()) TR_BitVector(5, comp->trMemory(), stackAlloc);
      TR_BitVector *toSub = new (comp->trStackMemory()) TR_BitVector(5, comp->trMemory(), stackAlloc);

      edgeMap[edge->getId() * 2] = toAdd;
      edgeMap[edge->getId() * 2 + 1] = toSub;

      bool forward = edge->getFrom()->asBlock() == block;
      CFGPredecessorIterator pit(comp->getFlowGraph(), loopBack, block);
      for (TR::CFGEdge *itr = pit.getFirst(); itr; itr = pit.getNext())  
         {
         if (itr != edge)
            forward ? addEdgeToEdge(edge, itr) : subEdgeFromEdge(edge, itr);
         }
      CFGSuccessorIterator sit(comp->getFlowGraph(), loopBack, block);
      for (TR::CFGEdge *itr = sit.getFirst(); itr; itr = sit.getNext())  
         {
         if (itr != edge)
            forward ? subEdgeFromEdge(edge, itr) : addEdgeToEdge(edge, itr);
         }

      if (trace)
         {
         traceMsg(comp, "edge %d-->%d:\n", edge->getFrom()->asBlock()->getNumber(), edge->getTo()->asBlock()->getNumber());
         printEdge(toAdd, toSub);
         }
      }

   void addEdgeToEdge(TR::CFGEdge *target, TR::CFGEdge *toAdd)
      {
      if (!hasEdge(target) || !hasEdge(toAdd))
         return;
      TR_BitVectorIterator bvi(*edgeMap[toAdd->getId() * 2]);
      while (bvi.hasMoreElements())
         {
         int32_t blockNum = bvi.getNextElement();
         if (!edgeMap[target->getId() * 2 + 1]->isSet(blockNum))
            edgeMap[target->getId() * 2]->set(blockNum);
         else
            edgeMap[target->getId() * 2 + 1]->reset(blockNum);
         }
      bvi.setBitVector(*edgeMap[toAdd->getId() * 2 + 1]);
      while (bvi.hasMoreElements())
         {
         int32_t blockNum = bvi.getNextElement();
         if (!edgeMap[target->getId() * 2]->isSet(blockNum))
            edgeMap[target->getId() * 2 + 1]->set(blockNum);
         else
            edgeMap[target->getId() * 2]->reset(blockNum);
         }
      }

   void subEdgeFromEdge(TR::CFGEdge *target, TR::CFGEdge *toSub)
      {
      if (!hasEdge(target) || !hasEdge(toSub))
         return;
      TR_BitVectorIterator bvi(*edgeMap[toSub->getId() * 2]);
      while (bvi.hasMoreElements())
         {
         int32_t blockNum = bvi.getNextElement();
         if (!edgeMap[target->getId() * 2]->isSet(blockNum))
            edgeMap[target->getId() * 2 + 1]->set(blockNum);
         else
            edgeMap[target->getId() * 2]->reset(blockNum);
         }
      bvi.setBitVector(*edgeMap[toSub->getId() * 2 + 1]);
      while (bvi.hasMoreElements())
         {
         int32_t blockNum = bvi.getNextElement();
         if (!edgeMap[target->getId() * 2 + 1]->isSet(blockNum))
            edgeMap[target->getId() * 2]->set(blockNum);
         else
            edgeMap[target->getId() * 2 + 1]->reset(blockNum);
         }
      }

   bool computeBlockFrequencyFromPredecessors(TR_BitVector** blockCounters, TR_BlockFrequencyInfo *bfi, TR::Block *block)
      {
      TR_BitVector additive(5, comp->trMemory(), stackAlloc), subtractive(5, comp->trMemory(), stackAlloc);
      CFGPredecessorIterator predItr(comp->getFlowGraph(), loopBack, block);
      predItr.getFirst();
      while (predItr.getCurrent())
         {
         TR::CFGEdge *edge = predItr.getCurrent();
         if (!hasEdge(edge))
            break;
         predItr.getNext();
         }
      if (predItr.getCurrent() == NULL)
         {
         if (!blockCounters[block->getNumber() * 2])
            {
            predItr.getFirst();
            while (predItr.getCurrent())
               {
               TR::CFGEdge *edge = predItr.getCurrent();
               TR_BitVectorIterator bvi(*edgeMap[edge->getId() * 2]);
               while (bvi.hasMoreElements())
                  {
                  int32_t blockNum = bvi.getNextElement();
                  if (!subtractive.isSet(blockNum))
                     additive.set(blockNum);
                  else
                     subtractive.reset(blockNum);
                  }
               bvi.setBitVector(*edgeMap[edge->getId() * 2 + 1]);
               while (bvi.hasMoreElements())
                  {
                  int32_t blockNum = bvi.getNextElement();
                  if (!additive.isSet(blockNum))
                      subtractive.set(blockNum);
                   else
                      additive.reset(blockNum);
                  }
               predItr.getNext();
               }
            computeBlockFrequency(block->getNumber(), blockCounters, bfi, additive, subtractive);
            }
         return true;
         }
      /*else
         {
         traceMsg(comp, "Could not compute frequency for block_%d - unknown edge from block_%d\n", block->getNumber(), predItr.getCurrent()->getFrom()->asBlock()->getNumber());
         }*/
      return false;
      }

   bool computeBlockFrequencyFromSuccessors(TR_BitVector** blockCounters, TR_BlockFrequencyInfo *bfi, TR::Block *block)
      {
      TR_BitVector additive(5, comp->trMemory(), stackAlloc), subtractive(5, comp->trMemory(), stackAlloc);
      CFGSuccessorIterator succItr(comp->getFlowGraph(), loopBack, block);
      succItr.getFirst();
      while (succItr.getCurrent())
         {
         TR::CFGEdge *edge = succItr.getCurrent();
         if (!hasEdge(edge))
            break;
         succItr.getNext();
         }
      if (succItr.getCurrent() == NULL)
         {
         if (!blockCounters[block->getNumber() * 2])
            {
            succItr.getFirst();
            while (succItr.getCurrent())
               {
               TR::CFGEdge *edge = succItr.getCurrent();
               TR_BitVectorIterator bvi(*edgeMap[edge->getId() * 2]);
               while (bvi.hasMoreElements())
                  {
                  int32_t blockNum = bvi.getNextElement();
                  if (!subtractive.isSet(blockNum))
                     additive.set(blockNum);
                  else
                     subtractive.reset(blockNum);
                  }
               bvi.setBitVector(*edgeMap[edge->getId() * 2 + 1]);
               while (bvi.hasMoreElements())
                  {
                  int32_t blockNum = bvi.getNextElement();
                  if (!additive.isSet(blockNum))
                     subtractive.set(blockNum);
                  else
                     additive.reset(blockNum);
                  }
               succItr.getNext();
               }
            computeBlockFrequency(block->getNumber(), blockCounters, bfi, additive, subtractive);
            }
         return true;
         }
      /*else
         {
         traceMsg(comp, "Could not compute frequency for block_%d - unknown edge to block_%d\n", block->getNumber(), succItr.getCurrent()->getTo()->asBlock()->getNumber());
         }*/

      return false;
      }

   private:
   void computeBlockFrequency(int32_t blockNum, TR_BitVector** blockCounters, TR_BlockFrequencyInfo *bfi, TR_BitVector &additive, TR_BitVector &subtractive)
      {
      int32_t additiveElementCount = additive.elementCount();
      int32_t subtractiveElementCount = subtractive.elementCount();
      TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "jprofiling.counterSize/additive/%d", additiveElementCount));
      if (additiveElementCount > 1)
         {
         blockCounters[blockNum * 2] = new (comp->trPersistentMemory()) TR_BitVector(additive.elementCount(), comp->trMemory(), persistentAlloc);
         *(blockCounters[blockNum * 2]) = additive;
         }
      else
         {
         blockCounters[blockNum * 2] = (TR_BitVector*)((additive.getHighestBitPosition() << 1) + 1);
         }

      if (subtractiveElementCount > 0)
         {
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "jprofiling.counterSize/subtractive/%d", subtractiveElementCount));
         if (subtractiveElementCount > 1)
            {
            blockCounters[blockNum * 2 + 1] = new (comp->trPersistentMemory()) TR_BitVector(subtractive.elementCount(), comp->trMemory(), persistentAlloc);
            *(blockCounters[blockNum * 2 + 1]) = subtractive;
            }
         else
            {
            blockCounters[blockNum * 2 + 1] = (TR_BitVector*)((subtractive.getHighestBitPosition() << 1) + 1);
            }
         }
      }
   };

/**
 * Computes the minimum spanning tree across the CFG using Prim's algorithm
 * \param parent The parent map populated by Prim's algorithm which is the computed spanning tree stored as child-parent key-value pairs
 * \param Q The priority queue to be used to run the algorithm
 * \param stackMemoryRegion The stack memory from which to allocate temporary data structures
 */
void TR_JProfilingBlock::computeMinimumSpanningTree(BlockParents &parent, BlockPriorityQueue &Q, TR::StackMemoryRegion &stackMemoryRegion)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   BlockWeights weights(cfg->getNextNodeNumber(), stackMemoryRegion);
   TR::BlockChecklist inMST(comp());

   // Prim's init
      {
      TR::Block *first = optimizer()->getMethodSymbol()->getFirstTreeTop()->getNode()->getBlock();
      weights[first] = 0;
      Q.push(std::make_pair(0,first));
      }

   while (!Q.empty())
      {
      TR::Block *block = Q.top().second;
      Q.pop();

      inMST.add(block);
      if (trace())
         traceMsg(comp(), "Add block_%d to the MST\n", block->getNumber());

      AdjacentBlockIterator adj(comp(), block);
      while (adj.current())
         {
         TR::Block *candidate = adj.current();
         if (trace())
            traceMsg(comp(), "  adj block_%d weight %d\n", candidate->getNumber(), adj.frequency());
         if (!inMST.contains(candidate) && weights[candidate] > -adj.frequency())
            {
            weights[candidate] = -adj.frequency();
            Q.push(std::make_pair(-adj.frequency(), candidate));
            parent[candidate] = block;
            }
         ++adj;
         }
      }   
   }

/**
 *
 * \param parent The child-parent key-value map holding the Minimum Spanning 
 *               Tree across the CFG which need not be counted
 * \param countedBlocks A list of the blocks which need to be counted to produce
 *               the method execution profile
 * \param loopBack The special loopBack edge which connects the method exit
 *               and enter blocks
 */
int32_t TR_JProfilingBlock::processCFGForCounting(BlockParents &parent, TR::BlockChecklist &countedBlocks, TR::CFGEdge &loopBack)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   int32_t firstNewBlockNumber = comp()->getFlowGraph()->getNextNodeNumber();
   int32_t edgeId = 0;
   
   for (CFGNodeIterator iter(cfg, this); iter.currentBlock() != NULL; ++iter)
      {
      TR::Block *block = iter.currentBlock();
      
      if (block->getNumber() >= firstNewBlockNumber)
         continue;
     
      CFGSuccessorIterator sit(cfg, &loopBack, block);
      for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
         {
         edge->setId(edgeId++);
         TR::Block *to = edge->getTo()->asBlock();
     
         // check if the spanning tree says we can omit this edge
         if (block != to)
            {
            if (parent[block] == to)
               {
               if (trace())
                  traceMsg(comp(), "skipping edge block_%d to block_%d\n", block->getNumber(), to->getNumber());
               continue;
               }
     
            if (parent[to] == block
                && !(to->hasSuccessor(block) && block->hasSuccessor(to)))
               {
               if (trace())
                  traceMsg(comp(), "skipping edge block_%d to block_%d\n", block->getNumber(), to->getNumber());
               continue;
               }
            }

         // if we are here we have an edge we need to count so determine where the counter goes
         TR::Block *insertionBlock = NULL;

         // Any block may have an edge to an OSRCatchBlock, but this block cannot container a counter as it
         // never executes, instead count the OSR code block, its only successor.
         if (to->isOSRCatchBlock())
            {
            TR_ASSERT(to->getSuccessors().size() == 1, "OSRCatchBlock must have a single successor");
            TR::Block *codeBlock = to->getSuccessors().front()->getTo()->asBlock();
            TR_ASSERT(codeBlock->isOSRCodeBlock(), "OSRCatchBlock successor must be an OSRCodeBlock");
            if (!countedBlocks.contains(codeBlock))
               {
               countedBlocks.add(codeBlock);
               insertionBlock = codeBlock;
               if (trace())
                  traceMsg(comp(), "count osr block_%d (predecessor of block_%d)\n", codeBlock->getNumber(), to->getNumber());
               }
            }
         // if the source of the edge has a single source we can count the source
         else if (block != to && !block->isOSRCatchBlock() &&
             (block->getSuccessors().size() == 1 && block->getExceptionSuccessors().size() == 0)
             || (block->getSuccessors().size() == 0 && block->getExceptionSuccessors().size() == 1)
             || block->isOSRInduceBlock() || block->isOSRCodeBlock())
            {
            if (!countedBlocks.contains(block))
               {
               countedBlocks.add(block);
               insertionBlock = block;
               if (trace())
                  traceMsg(comp(), "count block_%d (single predecessor of block_%d)\n", block->getNumber(), to->getNumber());
               }
            }
         // if the destination of the edge has a single destination we can count the destination
         // if the destination is a catch block we also just count the destination
         else if (block != to &&
             (to->getPredecessors().size() == 1 && to->getExceptionPredecessors().size() == 0)
             || (to->getPredecessors().size() == 0 && to->getExceptionPredecessors().size() == 1)
             || to->isCatchBlock() || to->isOSRInduceBlock() || to->isOSRCodeBlock())
            {
            if (!countedBlocks.contains(to))
               {
               countedBlocks.add(to);
               insertionBlock = to;
               if (trace())
                  traceMsg(comp(), "count block_%d (single successor of block_%d)\n", to->getNumber(), block->getNumber());
               }
            }
         // we have a critical edge and so need to split it to insert the counter
         // the only exception being for exception edges where we want to just count the catch for performance
         else
            {
            insertionBlock = block->splitEdge(block, to, comp(), NULL, true);
            countedBlocks.add(insertionBlock);
            if (trace())
               traceMsg(comp(), "split edge %d to %d -- new block_%d\n", block->getNumber(), to->getNumber(), insertionBlock->getNumber());
            // the edge we originally numbered has gone so we need to decrement the counter
            edgeId -= 1;
            // now we number all of the predecessors and successors of the new block
            insertionBlock->getPredecessors().front()->setId(edgeId++);
            insertionBlock->getSuccessors().front()->setId(edgeId++);
            for (TR::CFGEdgeList::iterator itr = insertionBlock->getExceptionSuccessors().begin(), end = insertionBlock->getExceptionSuccessors().end(); itr != end; ++itr)
               (*itr)->setId(edgeId++);
            }
         }
      }

   // if we chose to count the start or end block, we count the first real blcok of the method instead
   if (countedBlocks.contains(cfg->getStart()->asBlock()))
      {
      countedBlocks.add(comp()->getStartBlock());
      countedBlocks.remove(cfg->getStart()->asBlock());
      }
   if (countedBlocks.contains(cfg->getEnd()->asBlock()))
      {
      countedBlocks.add(comp()->getStartBlock());
      countedBlocks.remove(cfg->getStart()->asBlock());
      }
   return edgeId;
   }

/**
 * Initialize the persistent data structurs used to store and exploit the method profiling data
 *
 * \return The block frequency information data structure holding the profiling counters
 */
TR_BlockFrequencyInfo *TR_JProfilingBlock::initRecompDataStructures()
   {
   // If this is a profiling compilation, there may be an existing block
   // frequency profiler. Remove it to avoid excess overhead.
   TR_BlockFrequencyProfiler *bfp = comp()->getRecompilationInfo()->getBlockFrequencyProfiler();
   if (bfp)
      comp()->getRecompilationInfo()->removeProfiler(bfp);
 
   TR_PersistentProfileInfo *profileInfo = comp()->getRecompilationInfo()->findOrCreateProfileInfo();
   return profileInfo->findOrCreateBlockFrequencyInfo(comp());
   }

/**
 * A debug method to print how to derive the block frequency of each block from
 * the counters which have been inserted into the compiled method body
 * \param componentCounters The counter data structure to print
 */
void TR_JProfilingBlock::dumpCounterDependencies(TR_BitVector **componentCounters)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   for (CFGNodeIterator iter(cfg, this); iter.currentBlock() != NULL; ++iter)
      {
      TR::Block *block = iter.currentBlock();
      if (cfg->getStart() == block || cfg->getEnd() == block)
         traceMsg(comp(), "block_%d:\n", block->getNumber());
      else
         traceMsg(comp(), "block_%d [%d,%d]:\n", block->getNumber(), block->getEntry()->getNode()->getInlinedSiteIndex(), block->getEntry()->getNode()->getByteCodeIndex());

      TR_BitVector *additive = componentCounters[block->getNumber()*2];
      traceMsg(comp(), "   add: ");
      if (((uintptr_t)additive & 0x1) == 1)
         {
         traceMsg(comp(), "%d ", ((uintptr_t)additive >> 1));
         }
      else if (!additive || additive->isEmpty())
         {
         traceMsg(comp(), "none");
         }
      else
         {
         TR_BitVectorIterator bvi(*additive);
         while (bvi.hasMoreElements())
            traceMsg(comp(), "%d ", bvi.getNextElement());
         }
      traceMsg(comp(), "\n");
      TR_BitVector *subtractive = componentCounters[iter.currentBlock()->getNumber()*2 + 1];
      traceMsg(comp(), "   sub: ");
      if (((uintptr_t)subtractive & 0x1) == 1)
         {
         traceMsg(comp(), "%d ", ((uintptr_t)subtractive >> 1));
         }
      else if (!subtractive || subtractive->isEmpty())
         {
         traceMsg(comp(), "none");
         }
      else
         {
         TR_BitVectorIterator bvi(*subtractive);
         while (bvi.hasMoreElements())
            traceMsg(comp(), "%d ", bvi.getNextElement());
         }
      traceMsg(comp(), "\n");
      }
   }

/**
 * Add runtime tests to the start of the method to trigger method recompilation once the
 * appropriate number of method entries has occurred as determined by the raw block
 * count of the first block of the method.
 */   
void TR_JProfilingBlock::addRecompilationTests(TR_BlockFrequencyInfo *blockFrequencyInfo)
   {
  // add invocation check to the top of the method
   int32_t *thresholdLocation = NULL;
   if (comp()->getMethodSymbol()->mayHaveNestedLoops())
      thresholdLocation = &nestedLoopRecompileThreshold;
   else if (comp()->getMethodSymbol()->mayHaveLoops())
      thresholdLocation = &loopRecompileThreshold;
   else
      thresholdLocation = &recompileThreshold;

   int32_t startBlockNumber = comp()->getStartBlock()->getNumber();
   blockFrequencyInfo->setEntryBlockNumber(startBlockNumber);
   TR::Node *node = comp()->getMethodSymbol()->getFirstTreeTop()->getNode();
   TR::Node *root = blockFrequencyInfo->generateBlockRawCountCalculationSubTree(comp(), startBlockNumber, node);
   bool isProfilingCompilation = comp()->isProfilingCompilation();
   if (root != NULL)
      {
      TR::Block * originalFirstBlock = comp()->getStartBlock();

      TR::Block *guardBlock1 = TR::Block::createEmptyBlock(node, comp(), originalFirstBlock->getFrequency());
         {
         // If this is profiling compilation we do not need to check if jProfiling is enabled or not at runtime,
         // In this case we only check if we have queued for recompilation before comparing against method invocation count.
         int32_t *loadAddress = isProfilingCompilation ? blockFrequencyInfo->getIsQueuedForRecompilation() : blockFrequencyInfo->getEnableJProfilingRecompilation();
         TR::SymbolReference *symRef = comp()->getSymRefTab()->createKnownStaticDataSymbolRef(loadAddress, TR::Int32);
         TR::Node *enableLoad = TR::Node::createWithSymRef(node, TR::iload, 0, symRef);
         TR::Node *enableTest = TR::Node::createif(TR::ificmpeq, enableLoad, TR::Node::iconst(node, -1), originalFirstBlock->getEntry());
         TR::TreeTop *enableTree = TR::TreeTop::create(comp(), enableTest);
         enableTest->setIsProfilingCode();
         guardBlock1->append(enableTree);
         }

      static int32_t jProfilingCompileThreshold = comp()->getOptions()->getJProfilingMethodRecompThreshold();
      if (trace())
         traceMsg(comp(),"Profiling Compile Threshold for method = %d\n",isProfilingCompilation ? jProfilingCompileThreshold : *thresholdLocation);
      TR::Block *guardBlock2 = TR::Block::createEmptyBlock(node, comp(), originalFirstBlock->getFrequency());
      TR::Node *recompThreshold = isProfilingCompilation ? TR::Node::iconst(node, jProfilingCompileThreshold) : TR::Node::createWithSymRef(node, TR::iload, 0, comp()->getSymRefTab()->createKnownStaticDataSymbolRef(thresholdLocation, TR::Int32));
      TR::Node *cmpFlagNode = TR::Node::createif(TR::ificmplt, root, recompThreshold, originalFirstBlock->getEntry());
      TR::TreeTop *cmpFlag = TR::TreeTop::create(comp(), cmpFlagNode);
      cmpFlagNode->setIsProfilingCode();
      guardBlock2->append(cmpFlag);

      // construct call block
      const char * const dc1 = TR::DebugCounter::debugCounterName(comp(),
         "methodRecomp/(%s)", comp()->signature());
      TR::Block *callRecompileBlock = TR::Block::createEmptyBlock(node, comp(), UNKNOWN_COLD_BLOCK_COUNT);
      callRecompileBlock->setIsCold(true);
      TR::TreeTop *callTree = TR::TransformUtil::generateRetranslateCallerWithPrepTrees(node, TR_PersistentMethodInfo::RecompDueToJProfiling, comp());
      callTree->getNode()->setIsProfilingCode();
      callRecompileBlock->append(callTree);
      TR::DebugCounter::prependDebugCounter(comp(), dc1, callTree);
      
      comp()->getRecompilationInfo()->getJittedBodyInfo()->setUsesJProfiling();
      TR::CFG *cfg = comp()->getFlowGraph();
      if (trace()) traceMsg(comp(), "adding edge start to guard\n");
         cfg->addEdge(cfg->getStart(), guardBlock1);
      if (trace()) traceMsg(comp(), "insert before guard to bump\n");
         cfg->insertBefore(guardBlock1, guardBlock2);
         cfg->insertBefore(guardBlock2, callRecompileBlock);
      if (trace()) traceMsg(comp(), "insertbefore call to original\n");
         cfg->insertBefore(callRecompileBlock, originalFirstBlock);
      
      if (trace()) traceMsg(comp(), "remove start to original\n");
         cfg->removeEdge(cfg->getStart(), originalFirstBlock);
      if (trace()) traceMsg(comp(), "set first\n");
         comp()->getJittedMethodSymbol()->setFirstTreeTop(guardBlock1->getEntry());
      if (trace())
         comp()->dumpMethodTrees("Trees after JProfiling");
      }
   else
      {
      TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "jprofiling.instrument/badcounters/(%s)", comp()->signature()));
      }
   }

/**
 * JProfiling consists of two components: JProfilingValue & JProfilingBlock
 * JProfilingBlock will run early in the compilation, such that the CFG closely resembles the original program structure,
 * whilst the JProfilingValue pass runs later, as values may be introduced, eliminated or simplified.
 *
 * For non-profiling compilations, this can be enabled with the option TR_EnableJProfiling. Other control infrastructure
 * may limit which compilations this is actually applied to. See the env option TR_DisableFilterOnJProfiling for more details.
 *
 * For profiling compilations, JProfiling can replace the existing JitProfiling instrumentation when either TR_EnableJProfiling
 * or TR_EnableJProfilingInProfilingCompilations are set. However, this introduces some complexity as a compilation may
 * switch to profiling part way though, potentially after JProfilingBlock should have run. In such a scenario, the compilation
 * will be restarted. See switchToProfiling() for the restart logic.
 */
int32_t TR_JProfilingBlock::perform() 
   {
   if (comp()->getOption(TR_EnableJProfiling))
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled, run JProfiling\n");
      }
   else if (comp()->getProfilingMode() == JProfiling)
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled for profiling compilations, run JProfilingBlock\n");
      }
   else
      {
      if (trace())
         traceMsg(comp(), "JProfiling has not been enabled, skip JProfilingBlock\n");
      comp()->setSkippedJProfilingBlock();
      return 0;
      }

   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "jprofiling.instrument/success/(%s)", comp()->signature()));

   TR::CFG *cfg = comp()->getFlowGraph();

   TR::CFGEdge loopBack;
   loopBack.setFrom(cfg->getEnd());
   loopBack.setTo(cfg->getStart());
   cfg->getEnd()->removeSuccessor(&loopBack);
   cfg->getStart()->removePredecessor(&loopBack);

   // From here, down, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   BlockParents parent(cfg->getNextNodeNumber(), stackMemoryRegion);
   BlockPriorityQueue Q( (BlockQueueCompare()), BlockQueueContainer(BlockContainerAllocator(stackMemoryRegion)));

   computeMinimumSpanningTree(parent, Q, stackMemoryRegion);

   // dump the spanning tree which is held in the parent map computed by computeMinimumSpanningTree
   if (trace())
      parent.dump(comp());

   TR::BlockChecklist countedBlocks(comp());
   int32_t numEdges = processCFGForCounting(parent, countedBlocks, loopBack);

   if (trace())
      comp()->dumpMethodTrees("Trees after JProfiling counter insertion");

   TR_BlockFrequencyInfo *blockFrequencyInfo = initRecompDataStructures();

   TR_BitVector** componentCounters = (TR_BitVector**)new (comp()->trMemory(), persistentAlloc, TR_Memory::BlockFrequencyInfo) void**[comp()->getFlowGraph()->getNextNodeNumber()*2]();
   blockFrequencyInfo->setCounterDerivationInfo(componentCounters);

   // now we need to derive the equations for the frequency of all edges and blocks in terms
   // of the counted blocks - first initialize edges trivially computable from directly counted blocks
   EdgeFrequencyInfo edgeInfo(comp(), &loopBack, numEdges, stackMemoryRegion, trace());
   for (CFGNodeIterator iter(cfg, this); iter.currentBlock() != NULL; ++iter)
      {
      TR::Block *block = iter.currentBlock();
      if (!countedBlocks.contains(block))
        {
        TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "jprofiling/uncounted/%s", comp()->getHotnessName(comp()->getMethodHotness())));
        continue;
        }

      componentCounters[block->getNumber()*2] = (TR_BitVector*)(((uintptr_t)block->getNumber() << 1) + 1);

      // add the actual counter to the block
      TR::SymbolReference *symRef = comp()->getSymRefTab()->createKnownStaticDataSymbolRef(blockFrequencyInfo->getFrequencyForBlock(block->getNumber()), TR::Int32);
      TR::TreeTop *tree = TR::TreeTop::createIncTree(comp(), block->getEntry()->getNode(), symRef, 1);
      tree->getNode()->setIsProfilingCode();
      block->prepend(tree);
      TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "jprofiling/globalCounted/%s", comp()->getHotnessName(comp()->getMethodHotness())));

      // Selection of counted blocks should have avoided this
      // OSR catch blocks are never executed, so adding a counter is pointless
      TR_ASSERT(!block->isOSRCatchBlock(), "should never try to count OSR catch block, as it is fake\n");

      // generate the immediate successors/predecessors dependencies when possible
      // and queue the next blocks to be processed onto priority queue Q
      if (block->getSuccessors().size() == 1 && (block->getExceptionSuccessors().size() == 0 || block->isOSRInduceBlock()))
         {
         TR::CFGEdge *edge = block->getSuccessors().front();
         edgeInfo.addAbsoluteEdge(edge, block);
         Q.push(std::make_pair(0, edge->getTo()->asBlock()));
         }
      else if (block->getSuccessors().size() == 0 && block->getExceptionSuccessors().size() == 1)
         {
         TR::CFGEdge *edge = block->getExceptionSuccessors().front();
         edgeInfo.addAbsoluteEdge(edge, block);
         Q.push(std::make_pair(0, edge->getTo()->asBlock()));
         }

      if (block->getPredecessors().size() == 1 && block->getExceptionPredecessors().size() == 0)
         {
         TR::CFGEdge *edge = block->getPredecessors().front();
         edgeInfo.addAbsoluteEdge(edge, block);
         Q.push(std::make_pair(0, edge->getFrom()->asBlock()));
         }
      else if (block->getPredecessors().size() == 0)
         {
         for (TR::CFGEdgeList::iterator itr = block->getExceptionPredecessors().begin(), end = block->getExceptionPredecessors().end(); itr != end; ++itr)
            {
            edgeInfo.addAbsoluteEdge(*itr, block);
            Q.push(std::make_pair(0, (*itr)->getFrom()->asBlock()));
            }
         }
      else if (block->isOSRInduceBlock() || block->isOSRCodeBlock())
         {
         for (TR::CFGEdgeList::iterator itr = block->getPredecessors().begin(), end = block->getPredecessors().end(); itr != end; ++itr)
            {
            TR::Block *from = (*itr)->getFrom()->asBlock();
            edgeInfo.addAbsoluteEdge(*itr, block);
            Q.push(std::make_pair(0, from));
            if (from->isOSRCatchBlock())
               {
               for (TR::CFGEdgeList::iterator itr2 = from->getPredecessors().begin(), end = from->getPredecessors().end(); itr2 != end; ++itr2)
                  {
                  edgeInfo.addAbsoluteEdge(*itr2, block);
                  Q.push(std::make_pair(0, (*itr2)->getFrom()->asBlock()));
                  }
               }
            }
         }
      }

   // process our work list of blocks deriving counters and queuing more blocks as we learn more
   while (!Q.empty())
      {
      TR::Block *block = Q.top().second;
      int32_t depth = Q.top().first;
      Q.pop();
   
      if (trace())
         traceMsg(comp(), "Processing depth %d - block_%d\n", depth, block->getNumber());
   
      if (edgeInfo.computeBlockFrequencyFromPredecessors(componentCounters, blockFrequencyInfo, block))
         {
         if (trace())
            traceMsg(comp(), "Can compute block_%d from predecessors\n", block->getNumber());
         bool canComputeParent = true;
         TR::CFGEdge *parentEdge = NULL;
         CFGSuccessorIterator sit(cfg, &loopBack, block);
         for (TR::CFGEdge *edge = sit.getFirst(); canComputeParent && edge; edge = sit.getNext())
            {
            if (!edgeInfo.hasEdge(edge))
               {
               if (!parentEdge)
                  parentEdge = edge;
               else
                  canComputeParent = false;
               }
            }
         if (parentEdge && canComputeParent)
            {
            edgeInfo.addEdge(parentEdge, block);
            Q.push(std::make_pair(depth + 1, parentEdge->getTo()->asBlock()));
            }
         }
      else if (edgeInfo.computeBlockFrequencyFromSuccessors(componentCounters, blockFrequencyInfo, block))
         {
         if (trace())
            traceMsg(comp(), "Can compute block_%d from successors\n", block->getNumber());
         bool canComputeParent = true;
         TR::CFGEdge *parentEdge = NULL;
         CFGPredecessorIterator pit(cfg, &loopBack, block);
         for (TR::CFGEdge *edge = pit.getFirst(); canComputeParent && edge; edge = pit.getNext())
            {
            if (!edgeInfo.hasEdge(edge))
               {
               if (!parentEdge)
                  parentEdge = edge;
               else
                  canComputeParent = false;
               }
            }
         if (parentEdge && canComputeParent)
            {
            edgeInfo.addEdge(parentEdge, block);
            Q.push(std::make_pair(depth + 1, parentEdge->getFrom()->asBlock()));
            }
         }
      }
      
   // dump counter dependency information
   if (trace())
      dumpCounterDependencies(componentCounters); 
   // modify the method to add tests to trigger recompilation at runtime
   addRecompilationTests(blockFrequencyInfo);
   return 1;
   }

const char *
TR_JProfilingBlock::optDetailString() const throw()
   {
   return "O^O JPROFILING BLOCK: ";
   }
