/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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

#ifndef TRJ9_X86_INSTRUCTION_INCL
#define TRJ9_X86_INSTRUCTION_INCL

#include "codegen/J9Instruction.hpp"

namespace TR
{
class Instruction;

class OMR_EXTENSIBLE Instruction : public J9::InstructionConnector
   {
   public:

   // TODO: need to fix the TR::InstOpCode initialization once TR::InstOpCode class is done

   /*
    * Generic constructors
    */
   inline Instruction(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg, OMR::X86::Encoding encoding = OMR::X86::Default);

   inline Instruction(TR::InstOpCode::Mnemonic op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg, OMR::X86::Encoding encoding = OMR::X86::Default);


   /*
    * X86 specific constructors, need to call initializer to perform proper construction
    */
   inline Instruction(TR::RegisterDependencyConditions *cond, TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg, OMR::X86::Encoding encoding = OMR::X86::Default);

   inline Instruction(TR::RegisterDependencyConditions *cond, TR::InstOpCode::Mnemonic op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg, OMR::X86::Encoding encoding = OMR::X86::Default);

   };

}

#include "codegen/J9Instruction_inlines.hpp"

TR::Instruction::Instruction(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg, OMR::X86::Encoding encoding) :
   J9::InstructionConnector(cg, TR::InstOpCode::bad, node)
   {
   self()->setOpCodeValue(op);
   self()->setEncodingMethod(encoding);
   self()->initialize();
   }

TR::Instruction::Instruction(TR::InstOpCode::Mnemonic op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg, OMR::X86::Encoding encoding) :
   J9::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::bad)
   {
   self()->setOpCodeValue(op);
   self()->setEncodingMethod(encoding);
   self()->initialize();
   }

TR::Instruction::Instruction(TR::RegisterDependencyConditions *cond, TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg, OMR::X86::Encoding encoding) :
   J9::InstructionConnector(cg, TR::InstOpCode::bad, node)
   {
   self()->setOpCodeValue(op);
   self()->setEncodingMethod(encoding);
   self()->setDependencyConditions(cond);
   self()->initialize(cg, cond, op, true);
   }

TR::Instruction::Instruction(TR::RegisterDependencyConditions *cond, TR::InstOpCode::Mnemonic op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg, OMR::X86::Encoding encoding) :
   J9::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::bad)
   {
   self()->setOpCodeValue(op);
   self()->setEncodingMethod(encoding);
   self()->setDependencyConditions(cond);
   self()->initialize(cg, cond, op);
   }


#endif
