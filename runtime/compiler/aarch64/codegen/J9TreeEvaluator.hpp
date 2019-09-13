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

#ifndef J9_ARM64_TREE_EVALUATOR_INCL
#define J9_ARM64_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { namespace ARM64 { class TreeEvaluator; } }
namespace J9 { typedef J9::ARM64::TreeEvaluator TreeEvaluatorConnector; }
#else
#error J9::ARM64::TreeEvaluator expected to be a primary connector, but a J9 connector is already defined
#endif


#include "compiler/codegen/J9TreeEvaluator.hpp"  // include parent
#include "il/symbol/LabelSymbol.hpp"

namespace J9
{

namespace ARM64
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *flushEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   /*
    * Generates instructions to fill in the J9JITWatchedStaticFieldData.fieldAddress, J9JITWatchedStaticFieldData.fieldClass for static fields,
    * and J9JITWatchedInstanceFieldData.offset for instance fields at runtime. Used for fieldwatch support.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.  
    */
   static void generateFillInDataBlockSequenceForUnresolvedField (TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister);

   /*
    * Generate instructions for static/instance field access report.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.  
    */
   static void generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister);

   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *preced=NULL);
   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *preced=NULL);

   };

}

}

#endif
