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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/SPMDPreCheck.hpp"
#include "il/ILOpCodes.hpp"                        // for ILOpCodes, etc
#include "il/ILOps.hpp"                            // for TR::ILOpCode, etc
#include "il/Node.hpp"                             // for Node, etc
#include "il/Node_inlines.hpp"
#include "codegen/CodeGenerator.hpp"

bool SPMDPreCheck::isSPMDCandidate(TR::Compilation *comp, TR_RegionStructure *loop)
   {
 
   if (!loop->isNaturalLoop())
      {
      traceMsg(comp, "SPMD PRE-CHECK FAILURE: region %d is not a natural loop and is discounted as an SPMD candidate\n", loop->getNumber());
       }

   TR_ScratchList<TR::Block> blocksInLoopList(comp->trMemory());
   loop->getBlocks(&blocksInLoopList);
   ListIterator<TR::Block> blocksIt(&blocksInLoopList);

   for (TR::Block *nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();

         // explicitly allowed opcodes
         switch (node->getOpCodeValue())
            {
            case TR::BBStart:
            case TR::BBEnd:
            case TR::asynccheck:
            case TR::compressedRefs:
               continue;
            }

          // explicitly allowed families of opcodes
          TR::ILOpCode &opcode = node->getOpCode();
          if (opcode.isBranch())
             continue;

          if (opcode.isStore())
             {
             TR::ILOpCodes vectorOp = TR::ILOpCode::convertScalarToVector(opcode.getOpCodeValue());
             if (vectorOp == TR::BadILOp)
                {
                traceMsg(comp, "SPMD PRE-CHECK FAILURE: store op code %s does not have a vector equivalent - skipping consideration of loop %d\n", comp->getDebug()->getName(opcode.getOpCodeValue()), loop->getNumber());
                return false;
                }
             if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOp, node->getDataType()))
                {
                traceMsg(comp, "SPMD PRE-CHECK FAILURE: vector op code %s is not supported on the current platform - skipping consideration of loop %d\n", comp->getDebug()->getName(vectorOp), loop->getNumber());
                return false;
                }
           
             continue;
             }

          // unsafe opcode - we will skip LAR and SPMD
          traceMsg(comp, "SPMD PRE-CHECK FAILURE: found disallowed treetop opcode %s at node %p in loop %d\n", comp->getDebug()->getName(node->getOpCodeValue()), node, loop->getNumber());
          return false;
          }
       }

   return true;
   }
