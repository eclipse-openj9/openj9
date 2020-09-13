/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "optimizer/abstractinterpreter/IDTBuilder.hpp"
#include "optimizer/abstractinterpreter/J9AbsInterpreter.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"


TR::CFG* J9::IDTBuilder::generateControlFlowGraph(TR_CallTarget* callTarget)
   {
   if (!_cfgGen)
      {
      _cfgGen = (TR_J9EstimateCodeSize *)TR_EstimateCodeSize::get(getInliner(), getInliner()->tracer(), 0);
      }
   
   TR::CFG* cfg = _cfgGen->generateCFG(callTarget, region()); //The cfg comes from ECS.

   //Configuring the CFG
   if (cfg) 
      {
      cfg->setFrequencies();
      TR::Block *endBlock = cfg->getEnd()->asBlock();

      // sum up the freq of the predecessors of the end block
      // Because the start block's frequency is not accurate. 
      // This sum should be equal to the start block frequency.
      int32_t freqSum = 0;
      for (auto e = endBlock->getPredecessors().begin(); e != endBlock->getPredecessors().end(); ++e)
         {
         freqSum += (*e)->getFrom()->asBlock()->getFrequency();
         }

      // set the start block fequency
      TR::Block* startBlock = cfg->getStart()->asBlock();
      startBlock->setFrequency(freqSum);

      // the successor of the start block
      (*startBlock->getSuccessors().begin())->getTo()->setFrequency(freqSum);
      }

   return cfg;
   }

J9::IDTBuilder::~IDTBuilder()
   {
   if (_cfgGen)
      {
      TR_EstimateCodeSize::release(_cfgGen);
      }
   }


void J9::IDTBuilder::performAbstractInterpretation(IDTNode* node, IDTBuilderVisitor& visitor, AbsArguments* args, int32_t callerIndex)
   {
   J9AbsInterpreter interpreter(node->getResolvedMethodSymbol(), node->getCallTarget()->_cfg, &visitor, args, region(), comp());
   
   interpreter.setCallerIndex(callerIndex);
   interpreter.interpret();

   node->setInliningMethodSummary(interpreter.getInliningMethodSummary());
   }
   