/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include <stdint.h>
#include "env/FrontEnd.hpp"
#include "env/j9method.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/HandleRecompilationOps.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"

int32_t
TR_HandleRecompilationOps::perform()
   {
   bool enableTransform = true;

   if (trace())
      {
      traceMsg(comp(), "Entering HandleRecompilationOps\n");
      }

   // Check the conditions that are required to allow OSR to be induced and
   // recompilation requested for unresolved value type operations.  Exit early
   // if any is not satisfied.
   //
   if (!TR::Compiler->om.areValueTypesEnabled() || !comp()->isOSRAllowedForOperationsRequiringRecompilation())
      {
      if (trace())
         {
         traceMsg(comp(), "Handle Recompilation Operations is not enabled:  areValueTypesEnabled == %d; isOSRAllowedForOperationsRequiringRecompilation == %d\n", TR::Compiler->om.areValueTypesEnabled(), comp()->isOSRAllowedForOperationsRequiringRecompilation());
         traceMsg(comp(), "Exiting HandleRecompilationOps\n");
         }

      return 0;
      }

   bool performedTransform = false;

   for (TR::TreeTop *tt = comp()->getStartTree(); tt != NULL; tt = tt->getNextTreeTop())
      {
      const bool transformedFromCurrentTree = visitTree(tt);
      performedTransform |= transformedFromCurrentTree;
      }

   if (trace())
     {
     traceMsg(comp(), "Exiting HandleRecompilationOps\n");
     }

   return performedTransform ? 1 : 0;
   }

bool
TR_HandleRecompilationOps::resolveCHKGuardsValueTypeOperation(TR::TreeTop *currTree, TR::Node *node)
   {
   TR::Node *resolveChild = node->getFirstChild();

   if (resolveChild->getOpCodeValue() == TR::loadaddr)
      {
      // Check whether the ResolveCHK guards a loadaddr that is used in a newvalue
      // operation.
      //
      TR::SymbolReference *loadAddrSymRef = resolveChild->getSymbolReference();

      if (loadAddrSymRef->isUnresolved())
         {
         for (TR::TreeTopIterator iter(currTree->getNextTreeTop(), comp());
              iter.currentNode()->getOpCodeValue() != TR::BBEnd;
              ++iter)
            {
            TR::Node *nextNode = iter.currentNode();
            if ((nextNode->getOpCode().isTreeTop() || nextNode->getOpCode().isCheck())
                && nextNode->getNumChildren() > 0)
               {
               nextNode = nextNode->getFirstChild();
               }

            if (nextNode->getOpCodeValue() == TR::newvalue)
               {
               TR::Node *classAddr = nextNode->getFirstChild();

               if (classAddr == resolveChild
                   || (classAddr->getOpCodeValue() == TR::loadaddr
                       && classAddr->getSymbolReference() == loadAddrSymRef))
                  {
                  return true;
                  }
               }
            }
         }
      }
   else if (resolveChild->getOpCode().isLoadVarOrStore() && resolveChild->getOpCode().isIndirect())
      {
      // Check whether the ResolveCHK guards a load or store operation on a field that is a Q type (value type)
      //
      TR::SymbolReference *symRef = resolveChild->getSymbolReference();

      if (symRef->getCPIndex() != -1 && static_cast<TR_ResolvedJ9Method*>(_methodSymbol->getResolvedMethod())->isFieldQType(symRef->getCPIndex()))
         {
         return true;
         }
      }

   return false;
   }

bool
TR_HandleRecompilationOps::visitTree(TR::TreeTop *currTree)
   {
   TR::Node *node = currTree->getNode();

   bool performedTransform = false;
   bool abortCompilation = false;

   TR::ILOpCode opcode = node->getOpCode();

   // Is this a ResolveCHK or ResolveAndNULLCHK that guards a value type operation?
   //
   if (opcode.isResolveCheck() && resolveCHKGuardsValueTypeOperation(currTree, node))
      {
      // We couldn't have found a value type operation if value types are not enabled.
      // The matching performed by resolveCHKGuardsValueTypeOperation must be in error if
      // value types are not enabled.
      //
      // Note that om.areValueTypesEnabled() is currently checked in the perform method,
      // but in case this optimization is ever needed in cases where value types are not
      // enabled, we might want to keep this assertion to watch for things that are
      // incorrectly identified as being value type operations.
      //
      TR_ASSERT_FATAL(TR::Compiler->om.areValueTypesEnabled(), "TR_HandleRecompilationOps thought node n%dn [%p] was part of a value types operation, but value types are not enabled.\n", node->getGlobalIndex(), node);

      if (performTransformation(comp(), "%sInserting induceOSR call after ResolveCHK node n%dn [%p]\n", optDetailString(), node->getGlobalIndex(), node))
         {
         performedTransform = true;
         TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOrCreateOSRMethodData(node->getByteCodeInfo().getCallerIndex(), _methodSymbol);

         if (comp()->getOption(TR_TraceILGen))
            {
            traceMsg(comp(), "Preparing to generate induceOSR for value type operation guarded by node n%dn [%p]\n", node->getGlobalIndex(), node);
            }

         TR::Node *branchNode = TR::Node::create(node, TR::Goto, 0);
         TR::TreeTop *branchTT = TR::TreeTop::create(comp(), branchNode);
         TR::TreeTop *lastTT = NULL;

         // Clean up trees following the point at which the branch to the induceOSR will be inserted,
         // as they will not be executed
         //
         TR::TreeTop *cleanupTT = currTree->getNextTreeTop();
         while (cleanupTT)
            {
            TR::Node *cleanupNode = cleanupTT->getNode();
            if (((cleanupNode->getOpCodeValue() == TR::athrow)
                 && cleanupNode->throwInsertedByOSR())
                || (cleanupNode->getOpCodeValue() == TR::BBEnd))
               {
               break;
               }

            TR::TreeTop *nextTT = cleanupTT->getNextTreeTop();
            currTree->join(nextTT);
            cleanupNode->recursivelyDecReferenceCount();
            cleanupTT = nextTT;
            }

         TR::Block *currBlock = currTree->getEnclosingBlock();
         TR::CFG *cfg = comp()->getFlowGraph();

         // Inserting an unconditional branch at the end of the current block,
         // so existing successors should be removed from the CFG
         for (auto nextEdge = currBlock->getSuccessors().begin(); nextEdge != currBlock->getSuccessors().end();)
            {
            cfg->removeEdge(*(nextEdge++));
            }

         if (!_methodSymbol->induceOSRAfterAndRecompile(currTree, node->getByteCodeInfo(), branchTT, false, 0, &lastTT))
            {
            if (trace())
               {
               traceMsg(comp(),  "Unable to generate induce OSR for ResolveCHK or ResolveAndNULLCHK node at node n%dn [%p].  Aborting compilation\n", node->getGlobalIndex(), node);
               }
            abortCompilation = true;
            }
         }
      else
         {
         abortCompilation = true;
         }

      if (abortCompilation)
         {
         // The optimization has been disabled either explicitly or because OSR is disabled or recompilation
         // is not allowed.  Emit a static debug counter to track the failure and abort the compilation,
         // as correct code generation will not be possible.
         //
         if (comp()->getOption(TR_TraceILGen))
            {
            traceMsg(comp(), "   Encountered ResolveCHK or ResolveAndNULLCHK node n%dn [%p], but cannot induce OSR.  performedTransform == %d.  Aborting compilation\n", node->getGlobalIndex(), node, performedTransform);
            }

         TR_ByteCodeInfo nodeBCI = node->getByteCodeInfo();

         if (nodeBCI.getCallerIndex() == -1)
            {
            TR::DebugCounter::incStaticDebugCounter(comp(),
               TR::DebugCounter::debugCounterName(comp(), "handleRecompilationOps.abort/(%s)/bc=%d",
                                                  comp()->signature(), nodeBCI.getByteCodeIndex()));
            }
         else
            {
            TR::DebugCounter::incStaticDebugCounter(comp(),
               TR::DebugCounter::debugCounterName(comp(), "handleRecompilationOps.abort/(%s)/bc=%d/root=(%s)",
                                                  comp()->getInlinedResolvedMethod(nodeBCI.getCallerIndex())->signature(trMemory()),
                                                  nodeBCI.getByteCodeIndex(), comp()->signature()));
            }

         comp()->failCompilation<TR::UnsupportedValueTypeOperation>("TR_HandleRecompilationOps encountered ResolveCHK or ResolveAndNULLCHK in node n%dn [%p] guarding value type operation, but transformation is not enabled", node->getGlobalIndex(), node);
         }
      }

   return performedTransform;
   }

const char *
TR_HandleRecompilationOps::optDetailString() const throw()
   {
   return "O^O HANDLE RECOMPILATION OPERATIONS:";
   }
