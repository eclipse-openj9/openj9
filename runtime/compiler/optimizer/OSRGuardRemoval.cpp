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
#include "optimizer/OSRGuardRemoval.hpp"

#include "compile/VirtualGuard.hpp"
#include "il/Block.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/OSRGuardAnalysis.hpp"
#include "ras/DebugCounter.hpp"

/**
 * Find an OSR guard between the provided treetop and the end of the
 * block. If the treetop will no longer be an yield and this function
 * returns true, requesting this optimization may increase performance.
 */
bool TR_OSRGuardRemoval::findMatchingOSRGuard(TR::Compilation *comp, TR::TreeTop *tt)
   {
   // Search for another yield before the end of the block
   tt = tt->getNextTreeTop();
   while (tt->getNode()->getOpCodeValue() != TR::BBEnd)
      {
      if (comp->isPotentialOSRPoint(tt->getNode()))
         return false;
      tt = tt->getNextTreeTop();
      }

   // If there was no yield between tt and the end of the block, check if there was an OSR guard
   tt = tt->getNode()->getBlock()->getLastRealTreeTop();
   if (tt->getNode()->isOSRGuard())
      return true;
   if (tt->getNode()->isTheVirtualGuardForAGuardedInlinedCall()
       && comp->cg()->supportsMergingGuards())
      {
      TR_VirtualGuard *guardInfo = comp->findVirtualGuardInfo(tt->getNode());
      return guardInfo->mergedWithOSRGuard();
      }

   return false;
   }

/**
 * This optimization improves performance under NextGenHCR by identifying OSR guards that are no longer
 * required due to optimizations that took place after OSRGuardInsertion.
 *
 * In NextGenHCR, OSR guards are inserted after certain yields to the VM, such as calls, monents and asyncchecks.
 * They must be placed there as the VM may decided during that yield that a method has been redefined. When execution
 * returns to the JITed code and the VM has announced a redefinition, the OSR guard will be patched and execution will
 * return to the VM immediately.
 *
 * However, if this yield has been removed, the OSR guard may no longer be necessary. For example, DAA could convert
 * a call into an equivalent tree. If this call had an OSR guard it could be removed as long as no other yield 
 * relied on it being there.
 */
int32_t TR_OSRGuardRemoval::perform()
   {
   // isPotentialOSRPoint will test if OSR infrastructure has been removed
   bool osrInfraRemoved = comp()->osrInfrastructureRemoved();
   comp()->setOSRInfrastructureRemoved(false);

   // Perform the analysis and invalidate structure before its manipulated to reduce cost
   TR_OSRGuardAnalysis guardAnalysis(comp(), optimizer(), comp()->getFlowGraph()->getStructure());

   bool modified = false;
   for (TR::Block *block = comp()->getStartBlock(); block != NULL; block = block->getNextBlock())
      {
      if (guardAnalysis.shouldSkipBlock(block))
         continue;

      // Whether or not this block contains an OSR guard, if yields reach it
      // or it contains a yield, there is nothing that can be done
      if (guardAnalysis.containsYields(block))
         {
         if (trace())
            traceMsg(comp(), "Skipping block_%d, contains yields\n", block->getNumber());
         continue;
         }
      if (!guardAnalysis._blockAnalysisInfo[block->getNumber()]->isEmpty())
         {
         if (trace())
            traceMsg(comp(), "Skipping block_%d, reaching yields\n", block->getNumber());
         continue;
         }

      TR::Node *node = block->getLastRealTreeTop()->getNode();
      if (node->isOSRGuard()
          && performTransformation(comp(), "O^O OSR GUARD REMOVAL: removing OSRGuard node n%dn\n", node->getGlobalIndex()))
         {
         if (!modified)
            {
            modified = true;
            comp()->getFlowGraph()->invalidateStructure();
            }

         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(node);
         comp()->removeVirtualGuard(guardInfo);
         block->removeBranch(comp());

         TR::DebugCounter::prependDebugCounter(
            comp(),
            TR::DebugCounter::debugCounterName(comp(), "osrGuardRemoval/successfulRemoval"),
            block->getExit());
         } 
      else if (node->isTheVirtualGuardForAGuardedInlinedCall()
          && comp()->cg()->supportsMergingGuards()
          && performTransformation(comp(), "O^O OSR GUARD REMOVAL: removing merged OSRGuard with VG node n%dn\n", node->getGlobalIndex()))
         {
         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(node);
         if (guardInfo->mergedWithOSRGuard())
            {
            if (!modified)
               {
               modified = true;
               comp()->getFlowGraph()->invalidateStructure();
               }

            guardInfo->setMergedWithOSRGuard(false);
            TR::DebugCounter::prependDebugCounter(
               comp(),
               TR::DebugCounter::debugCounterName(comp(), "osrGuardRemoval/successfulUnmerge"),
               block->getLastRealTreeTop()); 
            }
         }
      }

   comp()->setOSRInfrastructureRemoved(osrInfraRemoved);
   return modified;
   }

const char *
TR_OSRGuardRemoval::optDetailString() const throw()
   {
   return "O^O OSR GUARD REMOVAL: ";
   }
