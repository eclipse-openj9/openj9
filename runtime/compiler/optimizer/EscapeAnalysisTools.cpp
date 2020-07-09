/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

   // The symrefs provided through OSR liveness data are only valid at the
   // point of the OSR liveness analysis.  After transformations have been
   // applied to the trees, stores to those original symbols might have been
   // eliminated, so they cannot be relied upon directly on the eaEscapeHelper
   // call.  Instead, we use the DefiningMaps which provide a map from each
   // symref in the OSR liveness data to the symrefs whose definitions in the
   // current trees correspond to the definition of the original symref at the
   // point of the OSR liveness analysis.
   //
   // Setting the TR_DisableEAEscapeHelperDefiningMap environment variable
   // prevents the use of DefiningMaps in setting up the eaEscapeHelper call.
   // This might be used to help identify problems in the DefiningMaps, but
   // setting it will fall back to using the original OSR liveness data
   // instead, which itself may very well be incorrect.  Setting this
   // environment variable should not be relied upon as a work around.
   //
   TR_OSRMethodData *osrMethodData  = _comp->getOSRCompilationData()->getOSRMethodDataArray()[inlinedIndex + 1];
   static char *disableEADefiningMap = feGetEnv("TR_DisableEAEscapeHelperDefiningMap");
   DefiningMap *induceDefiningMap = !disableEADefiningMap ? osrMethodData->getDefiningMap() : NULL;

   if (_comp->trace(OMR::escapeAnalysis) && induceDefiningMap)
      {
      traceMsg(_comp, "definingMap at induceCall n%dn %d:%d\n", induceCall->getGlobalIndex(), induceCall->getByteCodeInfo().getCallerIndex(), induceCall->getByteCodeInfo().getByteCodeIndex());
      _comp->getOSRCompilationData()->printMap(induceDefiningMap);
      }

   // Gather all live autos and pending pushes at this point for inlined methods in _loads
   // This ensures objects that EA can stack allocate will be heapified if OSR is induced
   while (inlinedIndex > -1)
      {
      TR::ResolvedMethodSymbol *rms = _comp->getInlinedResolvedMethodSymbol(inlinedIndex);
      TR_ASSERT_FATAL(rms, "Unknown resolved method during escapetools");
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(inlinedIndex, rms);
      processAutosAndPendingPushes(rms, induceDefiningMap, methodData, byteCodeIndex);
      byteCodeIndex = _comp->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getByteCodeIndex();
      inlinedIndex = _comp->getInlinedCallSite(inlinedIndex)._byteCodeInfo.getCallerIndex();
      }

   // handle the outermost method
      {
      TR_OSRMethodData *methodData = osrCompilationData->findOSRMethodData(-1, _comp->getMethodSymbol());
      processAutosAndPendingPushes(_comp->getMethodSymbol(), induceDefiningMap, methodData, byteCodeIndex);
      }
   insertFakeEscapeForLoads(block, induceCall, _loads);
   }

void TR_EscapeAnalysisTools::processAutosAndPendingPushes(TR::ResolvedMethodSymbol *rms, DefiningMap *induceDefiningMap, TR_OSRMethodData *methodData, int32_t byteCodeIndex)
   {
   processSymbolReferences(rms->getAutoSymRefs(), induceDefiningMap, methodData->getLiveRangeInfo(byteCodeIndex));
   processSymbolReferences(rms->getPendingPushSymRefs(), induceDefiningMap, methodData->getPendingPushLivenessInfo(byteCodeIndex));
   }

void TR_EscapeAnalysisTools::processSymbolReferences(TR_Array<List<TR::SymbolReference>> *symbolReferences, DefiningMap *induceDefiningMap, TR_BitVector *deadSymRefs)
   {
   for (int i = 0; symbolReferences && i < symbolReferences->size(); i++)
      {
      List<TR::SymbolReference> autosList = (*symbolReferences)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
         {
         TR::Symbol *p = symRef->getSymbol();
         if ((p->isAuto() || p->isParm()) && p->getDataType() == TR::Address)
            {
            // If no DefiningMap is available for the current sym ref, or the
            // DefiningMap is empty, simply use the sym ref on the
            // eaEscapeHelper if it's live.  Otherwise, walk through the
            // DefiningMap and place all the sym refs for autos and parameters
            // whose definitions map to the sym ref from the OSR
            // liveness data that we're currently looking at.
            if (!induceDefiningMap
                || induceDefiningMap->find(symRef->getReferenceNumber()) == induceDefiningMap->end())
               {
               if (deadSymRefs == NULL || !deadSymRefs->isSet(symRef->getReferenceNumber()))
                  {
                  _loads->push_back(TR::Node::createWithSymRef(TR::aload, 0, symRef));
                  }
               }
            else
               {
               TR_BitVector *definingSyms = (*induceDefiningMap)[symRef->getReferenceNumber()];
               TR_BitVectorIterator definingSymsIt(*definingSyms);
               while (definingSymsIt.hasMoreElements())
                  {
                  int32_t definingSymRefNum = definingSymsIt.getNextElement();
                  TR::SymbolReference *definingSymRef = _comp->getSymRefTab()->getSymRef(definingSymRefNum);
                  TR::Symbol *definingSym = definingSymRef->getSymbol();

                  if ((definingSym->isAuto() || definingSym->isParm())
                      && (deadSymRefs == NULL || !deadSymRefs->isSet(definingSymRefNum)))
                     {
                     _loads->push_back(TR::Node::createWithSymRef(TR::aload, 0, definingSymRef));
                     }
                  }
               }
            }
         }
      }
   }
