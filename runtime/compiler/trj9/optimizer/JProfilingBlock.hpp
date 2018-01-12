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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#ifndef JPROFILINGBLOCK_INCL
#define JPROFILINGBLOCK_INCL

#include <stdint.h>                           // for int32_t
#include <queue>
#include <vector>
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

namespace TR { class Block; }
namespace TR { class BlockChecklist; }
class TR_BlockFrequencyInfo;

class BlockParents;

class TR_JProfilingBlock : public TR::Optimization
   {
   protected:
   typedef TR::typed_allocator<std::pair<int32_t, TR::Block *>, TR::Region & > BlockContainerAllocator;
   typedef std::vector<std::pair<int32_t, TR::Block *>, BlockContainerAllocator > BlockQueueContainer;
   typedef std::greater<std::pair<int32_t, TR::Block *>> BlockQueueCompare;
   typedef std::priority_queue< size_t, BlockQueueContainer, BlockQueueCompare > BlockPriorityQueue;

   public:
   static int32_t nestedLoopRecompileThreshold;
   static int32_t loopRecompileThreshold;
   static int32_t recompileThreshold;
   static int32_t profilingCompileThreshold;
   TR_JProfilingBlock(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_JProfilingBlock(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   protected:
   void computeMinimumSpanningTree(BlockParents &parents, BlockPriorityQueue &Q, TR::StackMemoryRegion &stackMemoryRegion);
   int32_t processCFGForCounting(BlockParents &parent, TR::BlockChecklist &countedBlocks, TR::CFGEdge &loopBack);
   TR_BlockFrequencyInfo *initRecompDataStructures();
   void dumpCounterDependencies(TR_BitVector **componentCounters);
   void addRecompilationTests(TR_BlockFrequencyInfo *blockFrequencyInfo, TR_BitVector **componentCounters);
   };

#endif
