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
#include "optimizer/HandleRecompilationOps.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"

int32_t
TR_HandleRecompilationOps::perform()
   {
   _enableTransform = true;

   if (trace())
      {
      traceMsg(comp(), "Entering HandleRecompilationOps\n");
      }

   // The following series of tests check whether voluntary OSR is allowed and that recompilation is allowed.
   // If not, _enableTransform is set to false, but the optimizer still checks for situations in which it
   // would like to induce OSR.  If any of those situations are encountered with _enableTransform set to
   // false, the compilation will abort, as correct code generation for those situations requires OSR to be
   // induced.
   //
   if (comp()->getOption(TR_DisableOSR))
      {
      if (trace())
         {
         traceMsg(comp(), "Disabling Handle Recompilation Operations as OSR is disabled\n");
         }

      _enableTransform = false;
      }

   if (comp()->getHCRMode() != TR::osr)
      {
      if (trace())
         {
         traceMsg(comp(), "Disabling Handle Recompilation Operations as HCR mode is not OSR\n");
         }

      _enableTransform = false;
      }

   if (comp()->getOSRMode() == TR::involuntaryOSR)
      {
      if (trace())
         {
         traceMsg(comp(), "Disabling Handle Recompilation Operations as OSR mode is involuntary\n");
         }

      _enableTransform = false;
      }

   if (!comp()->supportsInduceOSR())
      {
      if (trace())
         {
         traceMsg(comp(), "Disabling Handle Recompilation Operations as induceOSR is not supported\n");
         }

      _enableTransform = false;
      }

   if (!comp()->allowRecompilation())
      {
      if (trace())
         {
         traceMsg(comp(), "Disabling Handle Recompilation Operations as recompilation is not permitted\n");
         }

      _enableTransform = false;
      }

   for (TR::TreeTop *tt = comp()->getStartTree(); tt != NULL; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      visitNode(tt, node);
      }

   if (_enableTransform && trace())
     {
     traceMsg(comp(), "Exiting HandleRecompilationOps\n");
     }

   return 0;
   }

bool
TR_HandleRecompilationOps::resolveCHKGuardsValueTypeOperation(TR::TreeTop *currTree, TR::Node *node)
   {
   TR::Node *resolveChild = node->getFirstChild();

   if (resolveChild->getOpCodeValue() == TR::loadaddr)
      {
      // Check whether the ResolveCHK guards a loadaddr that is used in newvalue
      // operation.
      //
      TR::SymbolReference *loadAddrSymRef = resolveChild->getSymbolReference();

      if (loadAddrSymRef->isUnresolved())
         {
         for (TR::TreeTop *nextTree = currTree->getNextTreeTop();
              nextTree != NULL && nextTree->getNode()->getOpCodeValue() != TR::BBEnd;
              nextTree = nextTree->getNextTreeTop())
            {
            TR::Node *nextNode = nextTree->getNode();
            if (nextNode->getOpCode().isTreeTop() && nextNode->getNumChildren() > 0)
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

void
TR_HandleRecompilationOps::visitNode(TR::TreeTop *currTree, TR::Node *node)
   {
   TR::ILOpCode opcode = node->getOpCode();

   // Is this a ResolveCHK or ResolveAndNULLCHK that guards a value type operation?
   //
   if (opcode.isResolveCheck() && resolveCHKGuardsValueTypeOperation(currTree, node))
      {
      // We couldn't have found a value type operation if value types are not enabled.
      // The matching performed by resolveCHKGuardsValueTypeOperation must be in error if
      // value types are not enabled.
      //
      TR_ASSERT_FATAL(TR::Compiler->om.areValueTypesEnabled(), "TR_HandleRecompilationOps thought node n%dn [%p] was part of a value types operation, but value types are not enabled.\n", node->getGlobalIndex(), node);

      if (_enableTransform
          && performTransformation(comp(), "%sInserting induceOSR call after ResolveCHK node n%dn [%p]\n", optDetailString(), node->getGlobalIndex(), node))
         {
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

         TR_ASSERT_FATAL(_methodSymbol->induceOSRAfterAndRecompile(currTree, node->getByteCodeInfo(), branchTT, false, 0, &lastTT), "Unable to generate induce OSR");
         node->setSymbolReference(getSymRefTab()->findOrCreateResolveCheckSymbolRef(_methodSymbol));
         }
      else
         {
         // The optimization has been disabled either explicitly or because OSR is disabled or recompilation
         // is not allowed.  Emit a static debug counter to track the failure and abort the compilation,
         // as correct code generation will not be possible.
         //
         if (comp()->getOption(TR_TraceILGen))
            {
            traceMsg(comp(), "   Encountered ResolveCHK or ResolveAndNULLCHK node n%dn [%p], but cannot induce OSR.  Aborting compilation\n", node->getGlobalIndex(), node);
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
   }

const char *
TR_HandleRecompilationOps::optDetailString() const throw()
   {
   return "O^O HANDLE RECOMPILATION OPERATIONS:";
   }
