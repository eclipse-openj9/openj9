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

namespace TR { class IDTBuilder; }

namespace J9
{
class IDTBuilder : public OMR::IDTBuilderConnector
   {
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
   virtual TR::CFG* generateControlFlowGraph(TR_CallTarget* callTarget);

   /**
    * @brief Perform the abstract interpretation on the method in the IDTNode. 
    *
    * @param node the IDT node (method) to be interpreted
    * @param visitor the abstract interpretation visitor
    * @param arguments the arguments passed from the caller method.
    * @param callerIndex the caller index for call site
    */
   virtual void performAbstractInterpretation(IDTNode* node, IDTBuilderVisitor& visitor, AbsArguments* arguments, int32_t callerIndex);

   TR_J9EstimateCodeSize* _cfgGen;
   };
}
#endif
