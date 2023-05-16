/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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

#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "il/Block.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "optimizer/CatchBlockProfiler.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"

TR::CatchBlockProfiler::CatchBlockProfiler(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _catchBlockCounterSymRef(NULL)
   {}

int32_t TR::CatchBlockProfiler::perform()
   {
   if (comp()->getOption(TR_DisableEDO))
      {
      if (trace())
         traceMsg(comp(), "Catch Block Profiler is disabled because EDO is disabled\n");
      return 0;
      }
   TR::Recompilation *recompilation = comp()->getRecompilationInfo();
   if (!recompilation || !recompilation->couldBeCompiledAgain())
      {
      if (trace())
         traceMsg(comp(), "Catch Block Profiler is disabled because method cannot be recompiled\n");
      return 0;
      }

   if (trace())
      traceMsg(comp(), "Starting Catch Block Profiler\n");

   for (TR::Block * b = comp()->getStartBlock(); b; b = b->getNextBlock())
      if (!b->getExceptionPredecessors().empty() &&
          !b->isOSRCatchBlock() &&
          !b->isEmptyBlock()) // VP may have removed all trees from the block
         {
         if (performTransformation(comp(), "%s Add profiling trees to track the execution frequency of catch block_%d\n", optDetailString(), b->getNumber()))
            {
            if (!_catchBlockCounterSymRef)
               {
               uint32_t *catchBlockCounterAddress = recompilation->getMethodInfo()->getCatchBlockCounterAddress();
               _catchBlockCounterSymRef = comp()->getSymRefTab()->createKnownStaticDataSymbolRef(catchBlockCounterAddress, TR::Int32);
               _catchBlockCounterSymRef->getSymbol()->setIsCatchBlockCounter();
               _catchBlockCounterSymRef->getSymbol()->setNotDataAddress();
               }
            TR::TreeTop *profilingTree = TR::TreeTop::createIncTree(comp(), b->getEntry()->getNode(), _catchBlockCounterSymRef, 1, b->getEntry());
            profilingTree->getNode()->setIsProfilingCode();
            }
         }

   if (trace())
      traceMsg(comp(), "\nEnding Catch Block Profiler\n");
   return 1; // actual cost
   }

const char *
TR::CatchBlockProfiler::optDetailString() const throw()
   {
   return "O^O CATCH BLOCK PROFILER: ";
   }
