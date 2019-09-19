/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
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

   if (comp()->getOSRMode() != TR::voluntaryOSR)
      {
      if (comp()->trace(OMR::escapeAnalysis))
         {
         traceMsg(comp(), "Special handling of OSR points is not possible outside of voluntary OSR - nothing to do\n");
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

   _loads = new (comp()->trMemory()->currentStackRegion()) NodeDeque(NodeDequeAllocator(comp()->trMemory()->currentStackRegion()));

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
            _loads->clear();
            if (optimizer()->getUseDefInfo() != NULL)
               {
               optimizer()->setUseDefInfo(NULL);
               }
            if (optimizer()->getValueNumberInfo())
               {
               optimizer()->setValueNumberInfo(NULL);
               }
            insertFakePrepareForOSR(block, itr->getNode()->getFirstChild());
            break;
            }
         }
      }

   if (comp()->trace(OMR::escapeAnalysis))
      {
      comp()->dumpMethodTrees("Trees after Pre-Escape Analysis");
      }

   return 1;
   }

void TR_PreEscapeAnalysis::insertFakePrepareForOSR(TR::Block *block, TR::Node *induceCall)
   {
   TR_ByteCodeInfo &bci = induceCall->getByteCodeInfo();

   int32_t inlinedIndex = bci.getCallerIndex();
   int32_t byteCodeIndex = bci.getByteCodeIndex();
   TR_OSRCompilationData *osrCompilationData = comp()->getOSRCompilationData();

   // Gather all live autos and pending pushes at this point for inlined methods in _loads
   // This ensures objects that EA can stack allocate will be heapified if OSR is induced
   while (inlinedIndex > -1)
      {
      TR::ResolvedMethodSymbol *rms = comp()->getInlinedResolvedMethodSymbol(inlinedIndex);
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(inlinedIndex, rms);
      processAutosAndPendingPushes(rms, methodData, byteCodeIndex);
      byteCodeIndex = comp()->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getByteCodeIndex();
      inlinedIndex = comp()->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getCallerIndex();
      }

   // handle the outermost method
      {
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(-1, comp()->getMethodSymbol());
      processAutosAndPendingPushes(comp()->getMethodSymbol(), methodData, byteCodeIndex);
      }

   // Create fake call to prepareForOSR with references to all live autos and pending pushes
   TR::Node *fakePrepare = TR::Node::createWithSymRef(induceCall, TR::call, _loads->size(), comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_prepareForOSR, false, false, true));
   int idx = 0;
   for (auto itr = _loads->begin(), end = _loads->end(); itr != end; ++itr)
      {
      (*itr)->setByteCodeInfo(induceCall->getByteCodeInfo());
      fakePrepare->setAndIncChild(idx++, *itr);
      }
   dumpOptDetails(comp(), " Adding fake prepare n%dn to OSR induction block_%d\n", fakePrepare->getGlobalIndex(), block->getNumber());
   block->getLastRealTreeTop()->insertBefore(
      TR::TreeTop::create(comp(), TR::Node::create(induceCall, TR::treetop, 1, fakePrepare)));
   }

void TR_PreEscapeAnalysis::processAutosAndPendingPushes(TR::ResolvedMethodSymbol *rms, TR_OSRMethodData *methodData, int32_t byteCodeIndex)
   {
   processSymbolReferences(rms->getAutoSymRefs(), methodData->getLiveRangeInfo(byteCodeIndex));
   processSymbolReferences(rms->getPendingPushSymRefs(), methodData->getPendingPushLivenessInfo(byteCodeIndex));
   }

void TR_PreEscapeAnalysis::processSymbolReferences(TR_Array<List<TR::SymbolReference>> *symbolReferences, TR_BitVector *deadSymRefs)
   {
   for (int i = 0; symbolReferences && i < symbolReferences->size(); i++)
      {
      List<TR::SymbolReference> autosList = (*symbolReferences)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
         {
         TR::AutomaticSymbol *p = symRef->getSymbol()->getAutoSymbol();
         if (p && p->getDataType() == TR::Address && (deadSymRefs == NULL || !deadSymRefs->isSet(symRef->getReferenceNumber())))
            {
            _loads->push_back(TR::Node::createWithSymRef(TR::aload, 0, symRef));
            }
         }
      }
   }

const char *
TR_PreEscapeAnalysis::optDetailString() const throw()
   {
   return "O^O PRE ESCAPE ANALYSIS: ";
   }
