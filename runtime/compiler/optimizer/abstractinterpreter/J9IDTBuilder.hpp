/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_IDT_BUILDER_INCL
#define J9_IDT_BUILDER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_IDT_BUILDER_CONNECTOR
#define J9_IDT_BUILDER_CONNECTOR
namespace J9 { class IDTBuilder; }
namespace J9 { typedef J9::IDTBuilder IDTBuilderConnector; }
#endif

#include "optimizer/abstractinterpreter/OMRIDTBuilder.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include <vector>

namespace TR { class IDTBuilder; }

namespace J9
{
class OMR_EXTENSIBLE IDTBuilder : public OMR::IDTBuilderConnector
   {

   friend class OMR::IDTBuilder;

   public:
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, TR_InlinerBase* inliner) :
      OMR::IDTBuilderConnector(symbol, budget, region, comp, inliner),
      _cfgGen(NULL)
   {}

   ~IDTBuilder();

   private:

   /**
    * @brief generate the control flow graph of a call target so that the abstract interpretation can use.
    *
    * @param callTarget the call target to generate CFG for
    *
    * @return the generated CFG
    */
   TR::CFG* generateControlFlowGraph(TR_CallTarget* callTarget);

   /**
    * @brief Perform the abstract interpretation on the method in the IDTNode.
    *
    * @param node the IDT node (method) to be interpreted
    * @param visitor the abstract interpretation visitor
    * @param arguments the arguments passed from the caller method.
    */
   void performAbstractInterpretation(TR::IDTNode* node, OMR::IDTBuilder::Visitor& visitor, TR::vector<TR::AbsValue*, TR::Region&>* arguments);

   TR_J9EstimateCodeSize* _cfgGen;
   };
}
#endif
