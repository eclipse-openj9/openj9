/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/PreEscapeAnalysis.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "optimizer/EscapeAnalysisTools.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/OptimizationManager.hpp"

int32_t TR_PreEscapeAnalysis::perform()
   {
   if (!optimizer()->isEnabled(OMR::escapeAnalysis))
      {
      if (comp()->trace(OMR::escapeAnalysis))
         {
         traceMsg(comp(), "EscapeAnalysis is disabled - skipping Pre-EscapeAnalysis\n");
         }
      return 0;
      }

   if (comp()->getOSRMode() != TR::voluntaryOSR || comp()->getOption(TR_DisableOSRLiveRangeAnalysis))
      {
      if (comp()->trace(OMR::escapeAnalysis))
         {
         traceMsg(comp(), "Special handling of OSR points is not possible outside of voluntary OSR or if OSR Liveness is not available - nothing to do\n");
         }
      return 0;
      }
   if (optimizer()->getOptimization(OMR::escapeAnalysis)->numPassesCompleted() > 0)
      {
      if (comp()->trace(OMR::escapeAnalysis))
         {
         traceMsg(comp(), "EA has self-enabled, setup not required on subsequent passes - skipping preEscapeAnalysis\n");
         }
      return 0;
      }

   // Gather map of sym refs that were known during OSR Liveness analysis to
   // the sym refs that occur in the current trees
   //
   static char *disableEADefiningMap = feGetEnv("TR_DisableEAEscapeHelperDefiningMap");

   TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

   if (!disableEADefiningMap && comp()->getOSRCompilationData())
      {
      comp()->getOSRCompilationData()->buildDefiningMap(comp()->trMemory()->currentStackRegion());
      }

   TR_EscapeAnalysisTools tools(comp());
   for (TR::Block *block = comp()->getStartBlock(); block != NULL; block = block->getNextBlock())
      {
      if (!block->isOSRInduceBlock())
         continue;

      for (TR::TreeTop *itr = block->getEntry(), *end = block->getExit(); itr != end; itr = itr->getNextTreeTop())
         {
         if (itr->getNode()->getNumChildren() == 1
             && itr->getNode()->getFirstChild()->getOpCodeValue() == TR::call
             && itr->getNode()->getFirstChild()->getSymbolReference()->isOSRInductionHelper())
            {
            //_loads->clear();
            if (optimizer()->getUseDefInfo() != NULL)
               {
               optimizer()->setUseDefInfo(NULL);
               }
            if (optimizer()->getValueNumberInfo())
               {
               optimizer()->setValueNumberInfo(NULL);
               }
            tools.insertFakeEscapeForOSR(block, itr->getNode()->getFirstChild());
            break;
            }
         }
      }

   if (!disableEADefiningMap && comp()->getOSRCompilationData())
      {
      // Must discard references to the DefiningMaps when finished with them
      comp()->getOSRCompilationData()->clearDefiningMap();
      }

   if (comp()->trace(OMR::escapeAnalysis))
      {
      comp()->dumpMethodTrees("Trees after Pre-Escape Analysis");
      }

   return 1;
   }

const char *
TR_PreEscapeAnalysis::optDetailString() const throw()
   {
   return "O^O PRE ESCAPE ANALYSIS: ";
   }
