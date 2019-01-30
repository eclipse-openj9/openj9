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

#ifndef JITPROFILER_INCL
#define JITPROFILER_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "il/Node.hpp"
#include "infra/Checklist.hpp"

namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

/*
 * Prototype of profiling in the jit during cold compiles
 */

class TR_JitProfiler : public TR::Optimization
   {
   public:
   TR_JitProfiler(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_JitProfiler(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   enum ProfilingBlocksBypassBranchFlag { addProfilingBypass, addInlineProfilingTrees };

   private:
   int32_t    performOnNode(TR::Node *node, TR::TreeTop *tt);
   TR::Block *appendBranchTree(TR::Node *profilingNode, TR::Block *currentBlock);
   TR::Block *createProfilingBlocks(TR::Node *profilingNode, TR::Block *ifBlock, int32_t bufferSizeRequired);
   void       addBranchProfiling(TR::Node *branchNode, TR::TreeTop* tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch);
   void       addInstanceProfiling(TR::Node *instanceNode, TR::TreeTop *tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch);
   void       addCallProfiling(TR::Node *callNode, TR::TreeTop *tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch);

 
   TR::CFG           *_cfg;
   TR::TreeTop       *_lastTreeTop;
   TR::NodeChecklist *_checklist;


   class ProfileBlockPair
      {
      public:
      ProfileBlockPair(TR::Block *trueBlock, TR::Block *falseBlock) { this->trueBlock = trueBlock; this->falseBlock = falseBlock; }

      TR::Block *trueBlock;
      TR::Block *falseBlock;
      };

   class ProfileBlockCreator
      {
      // This class handles the population of profiling blocks - blocks that contain trees to record profiling info
      // It holds a reference to a profiling block, and it's API allows for a standard profiling tree to be added
      // On deletion, it also adds a goto to the appropriate block.
      public:
      ProfileBlockCreator(TR_JitProfiler *jp, TR::Block *profilingBlock, TR::Block *successorBlock, TR::Node *profilingNode, int32_t currentOffset = 0);
      ~ProfileBlockCreator();

      void addProfilingTree(TR::ILOpCodes storeType, TR::Node *storeValue, int32_t storeWidth);
      ProfileBlockPair addConditionTree(TR::ILOpCodes conditionType, TR::Node *firstChild, TR::Node *secondChild);

      private:
      TR_JitProfiler *_jitProfiler;
      TR::Block      *_profilingBlock;
      TR::Block      *_successorBlock;
      TR::Node       *_profilingNode;
      TR::Node       *_cursor;
      int32_t         _currentOffset;
      bool            _createdConditional;
      };
   };

#endif
