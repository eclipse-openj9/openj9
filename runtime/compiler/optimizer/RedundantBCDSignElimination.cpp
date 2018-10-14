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

#include "optimizer/RedundantBCDSignElimination.hpp"

#include <stdint.h>                            // for int32_t, uint32_t, etc
#include <stdio.h>                             // for NULL, printf
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd
#include "compile/Compilation.hpp"             // for Compilation, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"                      // for ABitVector<>::Cursor, etc
#include "env/IO.hpp"                          // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                        // for Block, toBlock
#include "il/DataTypes.hpp"                    // for DataTypes::Int32, etc
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                        // for ILOpCode, TR::ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector, etc
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/TRCfgEdge.hpp"                 // for CFGEdge
#include "infra/TRCfgNode.hpp"                 // for CFGNode
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/Structure.hpp"             // for TR_RegionStructure, etc
#include "optimizer/UseDefInfo.hpp"            // for TR_UseDefInfo, etc
#include "ras/Debug.hpp"                       // for TR_DebugBase


TR_RedundantBCDSignElimination::TR_RedundantBCDSignElimination(TR::OptimizationManager *manager, bool useUseDefs)
   : TR::Optimization(manager)
   {
   // Ideally, UseDefInfo setting should match next opt that uses UseDefInfo (if any)
   // However, this opt requires doesNotRequireLoadsAsDefs, but globalVP does not
   // this will cause UseDefInfo to be invoked twice
   // If UseDefInfo is not created only local part of this opt will be done
   _useUseDefs = useUseDefs;
   }

int32_t
TR_RedundantBCDSignElimination::perform()
   {
   if (comp()->getOption(TR_DisableRedundantBCDSignElimination)) return 0;

   markNodesForSignRemoval();

   if (!_useUseDefs) return 1;

   TR_UseDefInfo *info = optimizer()->getUseDefInfo();

   if (!info) return 1;

   info->buildDefUseInfo();

   for (int32_t i = 0; i < info->getNumDefOnlyNodes(); i++)
      {
      int32_t useDefIndex = i + info->getFirstDefIndex();
      TR::Node *defNode = info->getNode(useDefIndex);
      if (defNode &&
          defNode->getOpCode().isStore() &&
          defNode->getType().isBCD())
         {
         TR_UseDefInfo::BitVector usesOfThisDef(comp()->allocator());
         info->getUsesFromDef(usesOfThisDef, i + info->getFirstDefIndex());

         if (usesOfThisDef.IsZero())
            continue;

         bool needsCleanSign = false;
         TR_UseDefInfo::BitVector::Cursor cursor(usesOfThisDef);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t useIndex = (int32_t) cursor + info->getFirstUseIndex();
            TR::Node *useNode = info->getNode(useIndex);
            if (useNode == 0 || useNode->getFutureUseCount() != 0)
               {
               needsCleanSign = true;
               break;
               }
            }

         if (!needsCleanSign)
            {
            if (defNode->getVisitCount() == comp()->getVisitCount())
                defNode->setVisitCount(comp()->getVisitCount() - 1);
            visitNodeForSignRemoval(defNode, true, comp()->getVisitCount(), false);

            }
         }
      }

   return 1;
   }

const char *
TR_RedundantBCDSignElimination::optDetailString() const throw()
   {
   return "O^O REDUNDANT BCD SIGN ELIMINATION: ";
   }

void
TR_RedundantBCDSignElimination::markNodesForSignRemoval()
   {
   vcount_t visitCount = comp()->incVisitCount();
   TR::TreeTop * tt;

   // initialize future use count
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      tt->getNode()->initializeFutureUseCounts(visitCount);

   // traverse the trees
   visitCount = comp()->incVisitCount();
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
       visitNodeForSignRemoval(tt->getNode(), true, visitCount);
   }


TR::Node *
TR_RedundantBCDSignElimination::visitNodeForSignRemoval(TR::Node *node, bool canRemoveBCDClean, vcount_t visitCount, bool needSignCleanChildren)
   {
   if (node->getVisitCount() == visitCount)
      return NULL;
   node->setVisitCount(visitCount);

   if ((node->getFutureUseCount() == 0 && node->getReferenceCount() != 0) ||
       node->getOpCode().isSimpleBCDClean() ||
       node->getOpCode().isBasicPackedArithmetic() ||
       node->getOpCode().isSetSign() ||
       node->isSetSignValueOnNode()
      )
      {
      needSignCleanChildren = false;
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (!needSignCleanChildren &&
          child->getType().isBCD() &&
          child->getFutureUseCount() != 0 &&
          (!child->hasAnyDecimalSignState() || child->getOpCode().isSimpleBCDClean()))
         {
         child->decFutureUseCount();

         // make sure if child's state has changed it is traversed again
         if (child->getFutureUseCount() == 0 &&
             child->getVisitCount() == visitCount)
            child->setVisitCount(visitCount - 1);
         }

      if (!needSignCleanChildren &&
          !node->castedToBCD() &&
          child->canRemoveArithmeticOperand() &&
          (!child->getOpCode().isSimpleBCDClean() || canRemoveBCDClean) &&
          performTransformation(comp(), "%s   Removing redundant %s (0x%p) under parent %s (0x%p)\n", optDetailString(), child->getOpCode().getName(), child, node->getOpCode().getName(), node))
         {
         TR::Node *modifyPrecision = TR::Node::create(TR::ILOpCode::modifyPrecisionOpCode(child->getDataType()), 1, child->getFirstChild());
         modifyPrecision->setDecimalPrecision(child->getDecimalPrecision());
         child->recursivelyDecReferenceCount();
         node->setAndIncChild(i, modifyPrecision);
         return child;
         }

      visitNodeForSignRemoval(child, canRemoveBCDClean, visitCount);
      }

   return NULL;
   }

