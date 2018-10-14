/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef J9_PPC_TREE_EVALUATOR_INCL
#define J9_PPC_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { namespace Power { class TreeEvaluator; } }
namespace J9 { typedef J9::Power::TreeEvaluator TreeEvaluatorConnector; }
#else
#error J9::Power::TreeEvaluator expected to be a primary connector, but a J9 connector is already defined
#endif


#include "compiler/codegen/J9TreeEvaluator.hpp"  // include parent

namespace J9
{

namespace Power
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static TR::Register *wrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *flushEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *VMcheckcastEvaluator2(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMinstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMinstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   
   static void restoreTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);
   static void buildArgsProcessFEDependencies(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);
   static TR::Register *retrieveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);
   static TR::Register *VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void genArrayCopyWithArrayStoreCHK(TR::Node *node, TR::CodeGenerator *cg);
   static void genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, TR::CodeGenerator *cg);
   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *preced=0);
   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *preced=0);
   static TR::Register *VMgenCoreInstanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg, bool isVMifInstanceOf, int32_t depIndex, int32_t numDep, TR::Node *depNode, bool needResult, bool needHelperCall, bool testEqualClass, bool testCache, bool testCastClassIsSuper, TR::LabelSymbol *doneLabel, TR::LabelSymbol *res0Label, TR::LabelSymbol *res1Label, bool branchOn1);
   static TR::Register *VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   };

}

}

#endif
