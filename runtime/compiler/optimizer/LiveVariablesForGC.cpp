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

#include "optimizer/LiveVariablesForGC.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

// Local live variables for GC
//
// Determine which locals containing collected references need to be initialized
// during the method prologue.

#define OPT_DETAILS "O^O LIVE VARIABLES FOR GC: "

TR_LocalLiveVariablesForGC::TR_LocalLiveVariablesForGC(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_LocalLiveVariablesForGC::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // The CFG is followed to the first GC point on each path. Any locals that can
   // reach a GC point without being initialized must be initialized to NULL during
   // the prologue of the method.
   //

   // Assign an index to each local that is a collected reference
   //
   _numLocals = 0;
   TR::AutomaticSymbol *p;
   ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      if (p->isCollectedReference())
         {
         if (debug("traceGCRefs"))
            dumpOptDetails(comp(), "Local #%2d is symbol at 0x%p\n",_numLocals,p);
         p->setLiveLocalIndex(_numLocals++, fe());
         }
      }

   if (_numLocals == 0)
      {
      return 0; // actual cost
      }

   comp()->incVisitCount();
   TR_BitVector localsToBeInitialized(_numLocals,trMemory(), stackAlloc);
   localsToBeInitialized.setAll(_numLocals);
   findGCPointInBlock(toBlock(comp()->getFlowGraph()->getStart()), localsToBeInitialized);

   // If any locals were initialized at all GC points, mark their symbols as
   // being initialized; they need not be initialized during method prologue.
   //
   locals.reset();
   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      if (p->isCollectedReference() &&
          (!comp()->getOption(TR_MimicInterpreterFrameShape) ||
           !comp()->areSlotsSharedByRefAndNonRef() ||
           p->isSlotSharedByRefAndNonRef()) &&
          !localsToBeInitialized.get(p->getLiveLocalIndex()))
         {
         if (performTransformation(comp(), "%sRemoving prologue initialization of local [%p]\n", OPT_DETAILS, p))
            p->setInitializedReference();
         }
      }

   return 1; // actual cost
   }

const char *
TR_LocalLiveVariablesForGC::optDetailString() const throw()
   {
   return "O^O LOCAL LIVE VARIABLES FOR GC: ";
   }

void TR_LocalLiveVariablesForGC::findGCPointInBlock(TR::Block *block, TR_BitVector &localsToBeInitialized)
   {
   if (block->getVisitCount() == comp()->getVisitCount())
      return;
   block->setVisitCount(comp()->getVisitCount());

   for (TR::TreeTop *treeTop = block->getEntry(); treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->canCauseGC())
         {
         // The state of the locals at this GC safe point represents the state
         // for all successors too.
         //
         return;
         }
      if (node->getOpCodeValue() == TR::astore)
         {
         TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
         if (local && local->isCollectedReference())
            localsToBeInitialized.reset(local->getLiveLocalIndex());
         }
      }

   // If we have reached the end of the block without finding a GC safe point,
   // visit the successors.
   //
   TR_BitVector myLocalsToBeInitialized(_numLocals,trMemory(), stackAlloc);
   TR_BitVector succLocalsToBeInitialized(_numLocals,trMemory(), stackAlloc);
   for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
      {
      succLocalsToBeInitialized = localsToBeInitialized;
      findGCPointInBlock(toBlock((*succ)->getTo()),succLocalsToBeInitialized);
      myLocalsToBeInitialized |= succLocalsToBeInitialized;
      if (myLocalsToBeInitialized == localsToBeInitialized)
         return; // Nothing more to be discovered
      }
   for (auto succ = block->getExceptionSuccessors().begin(); succ != block->getExceptionSuccessors().end(); ++succ)
      {
      succLocalsToBeInitialized = localsToBeInitialized;
      findGCPointInBlock(toBlock((*succ)->getTo()),succLocalsToBeInitialized);
      myLocalsToBeInitialized |= succLocalsToBeInitialized;
      if (myLocalsToBeInitialized == localsToBeInitialized)
         return; // Nothing more to be discovered
      }
   localsToBeInitialized = myLocalsToBeInitialized;
   }

// Global live variables for GC
//
// Determine which locals are live at the start of each basic block. This will be
// used by code generation to build GC stack maps.

TR_GlobalLiveVariablesForGC::TR_GlobalLiveVariablesForGC(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_GlobalLiveVariablesForGC::perform()
   {
   if (comp()->getOption(TR_MimicInterpreterFrameShape) && !comp()->areSlotsSharedByRefAndNonRef())
      {
      return 0;
      }

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Temporary until register maps are implemented - spill temps must be
   // included in the liveness analysis.
   //
   if (!comp()->useRegisterMaps())
      {
      cg()->lowerTrees();
      cg()->findAndFixCommonedReferences();
      }

   // Because only live locals are mapped for GC, there is normally no need to
   // make sure locals are cleared to NULL during method prologue.
   // However, we can have cases where GC-collected locals are live at the start
   // of the method. These locals will have to be cleared to NULL during method
   // prologue.
   // This can happen because of the way we treat non-inlined jsrs.
   //
   int32_t numLocals = 0;
   TR::AutomaticSymbol *p;
   ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      // Mark collected locals as initialized. We will reset this property for
      // any locals that are live at the start of the method.
      //
      if (p->isCollectedReference() &&
          (!comp()->getOption(TR_MimicInterpreterFrameShape) ||
           !comp()->areSlotsSharedByRefAndNonRef() ||
           p->isSlotSharedByRefAndNonRef()))
         p->setInitializedReference();
      ++numLocals;
      }

   if (comp()->getOption(TR_EnableAggressiveLiveness))
      {
      TR::ParameterSymbol *pp;
      ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
      for (pp = parms.getFirst(); pp != NULL; pp = parms.getNext())
         ++numLocals;
      }

   // Nothing to do if there are no locals
   //
   if (numLocals == 0)
      {
      return 0; // actual cost
      }

   TR_BitVector *liveVars = NULL;

   if ((comp()->getOption(TR_EnableOSR) && (comp()->getHCRMode() == TR::osr || comp()->getOption(TR_FullSpeedDebug))) || !cg()->getLiveLocals()) // under OSR existing live locals is likely computed without ignoring OSR uses
      {
      // Perform liveness analysis
      //
      bool ignoreOSRuses = false; // Used to be set to true but we cannot set this to true because a variable may not be live in compiled code but may still be needed (live) in the interpreter
      /* for mimicInterpreterShape, because OSR points can extend the live range of autos 
       * autos sharing the same slot in interpreter might end up with overlapped
       * live range if OSRUses are not ignored
       */
      if (comp()->getOption(TR_MimicInterpreterFrameShape)) 
         ignoreOSRuses = true;

      TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure(), ignoreOSRuses, NULL, false, comp()->getOption(TR_EnableAggressiveLiveness));

      for (TR::CFGNode *cfgNode = comp()->getFlowGraph()->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
         {
         TR::Block *block     = toBlock(cfgNode);
         int32_t blockNum    = block->getNumber();
         if (blockNum > 0 && liveLocals._blockAnalysisInfo[blockNum])
            {
            liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
            *liveVars = *liveLocals._blockAnalysisInfo[blockNum];
            block->setLiveLocals(liveVars);
            }
         }

      // Make sure the code generator knows there are live locals for blocks, and
      // create a bit vector of the correct size for it.
      //
      liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
      cg()->setLiveLocals(liveVars);
      }

   // See if any collected reference locals are live at the start of the block.
   // These will need to be initialized at method prologue.
   //
   liveVars = comp()->getStartBlock()->getLiveLocals();
   if (liveVars && !liveVars->isEmpty())
      {
      locals.reset();
      for (p = locals.getFirst(); p != NULL; p = locals.getNext())
         {
         if (p->isCollectedReference() &&
             liveVars->get(p->getLiveLocalIndex()))
            {
            if (performTransformation(comp(), "%s Local #%d is live at the start of the method\n",OPT_DETAILS, p->getLiveLocalIndex()))
               p->setUninitializedReference();
            }
         }
      }

   return 10; // actual cost
   }

const char *
TR_GlobalLiveVariablesForGC::optDetailString() const throw()
   {
   return "O^O GLOBAL LIVE VARIABLES FOR GC: ";
   }
