/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TRJ9_INSTRUCTION_INCL
#define TRJ9_INSTRUCTION_INCL

#include "codegen/ARMConditionCode.hpp"
#include "codegen/J9Instruction.hpp"

#include "codegen/RegisterDependency.hpp" // @@@@

namespace TR
{
class Instruction;

class OMR_EXTENSIBLE Instruction : public J9::InstructionConnector
   {

   public:

   // TODO: need to fix the InstOpCode initialization
   inline Instruction(TR::Node *node, TR::CodeGenerator *cg);

   inline Instruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg);

   inline Instruction(TR::Instruction   *precedingInstruction,
               TR_ARMOpCodes     op,
               TR::Node          *node,
               TR::CodeGenerator *cg);

   inline Instruction(TR_ARMOpCodes                       op,
               TR::Node                            *node,
               TR::RegisterDependencyConditions    *cond,
               TR::CodeGenerator                   *cg);

   inline Instruction(TR::Instruction                     *precedingInstruction,
               TR_ARMOpCodes                       op,
               TR::Node                            *node,
               TR::RegisterDependencyConditions    *cond,
               TR::CodeGenerator                   *cg);
   };

}

#include "codegen/J9Instruction_inlines.hpp"

TR::Instruction::Instruction(TR::Node *node, TR::CodeGenerator *cg)
   : J9::InstructionConnector(cg, InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(ARMOp_bad);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

TR::Instruction::Instruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg)
   : J9::InstructionConnector(cg, InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

TR::Instruction::Instruction(TR::Instruction   *precedingInstruction,
                         TR_ARMOpCodes     op,
                         TR::Node          *node,
                         TR::CodeGenerator *cg)
   : J9::InstructionConnector(cg, precedingInstruction, InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

TR::Instruction::Instruction(TR_ARMOpCodes                       op,
                         TR::Node                            *node,
                         TR::RegisterDependencyConditions    *cond,
                         TR::CodeGenerator                   *cg)
   : J9::InstructionConnector(cg, InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(cond);
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


TR::Instruction::Instruction(TR::Instruction                     *precedingInstruction,
                         TR_ARMOpCodes                       op,
                         TR::Node                            *node,
                         TR::RegisterDependencyConditions    *cond,
                         TR::CodeGenerator                   *cg)
   : J9::InstructionConnector(cg, precedingInstruction, InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(cond);
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


//TODO: these downcasts everywhere need to be removed
inline uint32_t        * toARMCursor(uint8_t *i) { return (uint32_t *)i; }

#endif
