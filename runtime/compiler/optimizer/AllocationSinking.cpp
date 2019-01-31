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

#include "optimizer/AllocationSinking.hpp"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "ras/Debug.hpp"


int32_t TR_AllocationSinking::perform()
   {
   if (comp()->getOptions()->realTimeGC()) // memory area can be changed by the arg eval call, so better not disturb things
      return 0;

   // Note: if the evaluation of constructor arguments contains control flow (ie. a ternary)
   // then the "new" and ctor call will be in different blocks, and this opt won't have the
   // desired effect.

   // We change the trees as we scan, which is often slightly gross, but in
   // this case it works reasonably well; because we only move trees downward,
   // if we scan upward, we will see the right things at the right time.
   //
   // Because trees may move as we go, we have to remember where to continue scanning.
   //
   TR::TreeTop *allocScanPoint;

   for (TR::TreeTop *allocTree = comp()->findLastTree(); allocTree; allocTree = allocScanPoint)
      {
      allocScanPoint = allocTree->getPrevTreeTop();
      TR::Node *allocation;
      if (allocTree->getNode()->getOpCodeValue() == TR::treetop && (allocation=allocTree->getNode()->getFirstChild())->getOpCodeValue() == TR::New)
         {
         if (trace())
            {
            traceMsg(comp(), "Found allocation %s\n", comp()->getDebug()->getName(allocation));
            printf("Allocation Sinking found allocation %s in %s\n", comp()->getDebug()->getName(allocation), comp()->signature());
            }

         // Scan down for the first actual use of this new
         //
         TR::TreeTop *flushToSink = NULL;
         vcount_t visitCount = comp()->incVisitCount();
         for (TR::TreeTop *useTree = allocTree->getNextTreeTop(); useTree && (useTree->getNode()->getOpCodeValue() != TR::BBEnd); useTree = useTree->getNextTreeTop())
            {
            TR::Node *useNode = useTree->getNode();
            if (useNode->getOpCodeValue() == TR::allocationFence && useNode->getAllocation() == allocation)
               {
               // This flush is only protecting this allocation, so we can move it down too
               //
               flushToSink = useTree;
               if (trace())
                  traceMsg(comp(), "   Sinking flush %s along with %s\n",
                     comp()->getDebug()->getName(flushToSink->getNode()),
                     comp()->getDebug()->getName(allocation));
               }
            else if (
                  (useTree->getNode()->containsNode(allocation, visitCount))
               || (useNode->getOpCodeValue() == TR::allocationFence && useNode->getAllocation() == NULL) // found a flush that *might* be protecting this allocation and others; time to give up
               || (trace() && !performTransformation(comp(), "O^O ALLOCATION SINKING: Moving allocation %s down past %s\n",
                     comp()->getDebug()->getName(allocation), comp()->getDebug()->getName(useTree->getNode())))
               ){
               if (allocTree->getNextTreeTop() == useTree)
                  {
                  if (trace())
                     traceMsg(comp(), "   Allocation %s is used immediately in %s; no sinking opportunity\n",
                        comp()->getDebug()->getName(allocation),
                        comp()->getDebug()->getName(useTree->getNode()));
                  break;
                  }
               {
                  // Allocation Sinking is skipped when the class is unresolved.
                  TR::Node* nodeNew = allocTree->getNode()->getFirstChild();
                  TR_ASSERT(nodeNew, "Local Opts, expected the first child of a treetop not to be null");
                  TR_ASSERT(nodeNew->getOpCode().isNew(), "Local Opts, expected the first child of a treetop to be a new");
                  TR_ASSERT(nodeNew->getFirstChild(), "Local Opts, expected the first child of a new not to be null");
                  TR_ASSERT(nodeNew->getFirstChild()->getOpCode().isLoadAddr(), "Local Opts, expected the first child of a new to be a loadaddr");
                  if (nodeNew->getFirstChild()->hasUnresolvedSymbolReference())
                     {
                     continue;
                     }
               }
               // Move the "new" right before the first use / flush
               // (Note that we skip the performTransformation here if trace()
               // is on because it would interfere with the finer-grained one.)
               //
               if (  trace()
                  || (!comp()->ilGenTrace() || performTransformation(comp(), "O^O ALLOCATION SINKING: Moving allocation %s down to %s\n",
                        comp()->getDebug()->getName(allocation), comp()->getDebug()->getName(useTree->getNode())))
                  ){
                  allocTree->unlink(false);
                  useTree->insertBefore(allocTree);
                  if (flushToSink)
                     {
                     flushToSink->unlink(false);
                     useTree->insertBefore(flushToSink);
                     if (trace())
                        traceMsg(comp(), "   Sank flush %s along with allocation %s\n", comp()->getDebug()->getName(flushToSink->getNode()), comp()->getDebug()->getName(allocation));
                     }
                  }
               break;
               }
            }
         }
      }

   return 0;
   }

const char *
TR_AllocationSinking::optDetailString() const throw()
   {
   return "O^O ALLOCATION SINKING: ";
   }
