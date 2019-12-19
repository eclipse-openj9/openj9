/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "optimizer/EscapeAnalysisTools.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/SymbolReference.hpp"
#include "il/AutomaticSymbol.hpp"

TR_EscapeAnalysisTools::TR_EscapeAnalysisTools(TR::Compilation *comp)
  {
  _loads = NULL;
  _comp = comp;
  }

void TR_EscapeAnalysisTools::insertFakeEscapeForLoads(TR::Block *block, TR::Node *node, NodeDeque *loads)
   {
   //TR::Node *fakePrepare = TR::Node::createWithSymRef(node, TR::call, loads->size(), _comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_prepareForOSR, false, false, true));
   TR::Node *fakePrepare = TR::Node::createEAEscapeHelperCall(node, loads->size());
   int idx = 0;
   for (auto itr = loads->begin(), end = loads->end(); itr != end; ++itr)
       {
       (*itr)->setByteCodeInfo(node->getByteCodeInfo());
       fakePrepare->setAndIncChild(idx++, *itr);
       }
   dumpOptDetails(_comp, " Adding fake prepare n%dn to OSR induction block_%d\n", fakePrepare->getGlobalIndex(), block->getNumber());
   block->getLastRealTreeTop()->insertBefore(
      TR::TreeTop::create(_comp, TR::Node::create(node, TR::treetop, 1, fakePrepare)));
   }

void TR_EscapeAnalysisTools::insertFakeEscapeForOSR(TR::Block *block, TR::Node *induceCall)
   {
   if (_loads == NULL)
      _loads = new (_comp->trMemory()->currentStackRegion()) NodeDeque(NodeDequeAllocator(_comp->trMemory()->currentStackRegion()));
   else   
      _loads->clear();

   TR_ByteCodeInfo &bci = induceCall->getByteCodeInfo();

   int32_t inlinedIndex = bci.getCallerIndex();
   int32_t byteCodeIndex = bci.getByteCodeIndex();
   TR_OSRCompilationData *osrCompilationData = _comp->getOSRCompilationData();

   // Gather all live autos and pending pushes at this point for inlined methods in _loads
   // This ensures objects that EA can stack allocate will be heapified if OSR is induced
   while (inlinedIndex > -1)
      {
      TR::ResolvedMethodSymbol *rms = _comp->getInlinedResolvedMethodSymbol(inlinedIndex);
      TR_ASSERT_FATAL(rms, "Unknwon resolved method during escapetools");
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(inlinedIndex, rms);
      processAutosAndPendingPushes(rms, methodData, byteCodeIndex);
      byteCodeIndex = _comp->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getByteCodeIndex();
      inlinedIndex = _comp->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getCallerIndex();
      }

   // handle the outter most method
      {
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(-1, _comp->getMethodSymbol());
      processAutosAndPendingPushes(_comp->getMethodSymbol(), methodData, byteCodeIndex);
      }
   insertFakeEscapeForLoads(block, induceCall, _loads);
   }

void TR_EscapeAnalysisTools::processAutosAndPendingPushes(TR::ResolvedMethodSymbol *rms, TR_OSRMethodData *methodData, int32_t byteCodeIndex)
   {
   processSymbolReferences(rms->getAutoSymRefs(), methodData->getLiveRangeInfo(byteCodeIndex));
   processSymbolReferences(rms->getPendingPushSymRefs(), methodData->getPendingPushLivenessInfo(byteCodeIndex));
   }

void TR_EscapeAnalysisTools::processSymbolReferences(TR_Array<List<TR::SymbolReference>> *symbolReferences, TR_BitVector *deadSymRefs)
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
