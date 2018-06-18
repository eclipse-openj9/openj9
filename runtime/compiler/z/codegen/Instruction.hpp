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

#ifndef TR_INSTRUCTION_INCL
#define TR_INSTRUCTION_INCL

#include "codegen/J9Instruction.hpp"

namespace TR
{
class Instruction;

class OMR_EXTENSIBLE Instruction : public J9::InstructionConnector
   {
   public:

   /*
    * Generic constructors
    */
   Instruction(TR::InstOpCode::Mnemonic    op,
            TR::Node          *n,
            TR::CodeGenerator *cg) :
            J9::InstructionConnector(cg, op, n) {}

   Instruction(TR::InstOpCode::Mnemonic    op,
               TR::Node          *n,
               TR::Instruction   *precedingInstruction,
               TR::CodeGenerator *cg):
               J9::InstructionConnector(cg, precedingInstruction, op, n) {}

   /*
    * Z specific constructors, need to call initializer to perform proper construction
    */
   inline Instruction(TR::Instruction *precedingInstruction,
                      TR::InstOpCode::Mnemonic op,
                      TR::RegisterDependencyConditions *cond,
                      TR::CodeGenerator *cg);

   inline Instruction(TR::InstOpCode::Mnemonic    op,
                      TR::Node          *n,
                      TR::RegisterDependencyConditions *cond,
                      TR::CodeGenerator *cg);


   inline Instruction(TR::InstOpCode::Mnemonic    op,
                      TR::Node          *n,
                      TR::RegisterDependencyConditions * cond,
                      TR::Instruction   *precedingInstruction,
                      TR::CodeGenerator *cg);

   };

}

#include "codegen/J9Instruction_inlines.hpp"

TR::Instruction::Instruction(TR::Instruction *precedingInstruction,
                             TR::InstOpCode::Mnemonic op,
                             TR::RegisterDependencyConditions *cond,
                             TR::CodeGenerator                    *cg) :
                             J9::InstructionConnector(cg, precedingInstruction, op) { self()->initialize(precedingInstruction, true, cond, false); }

TR::Instruction::Instruction(TR::InstOpCode::Mnemonic    op,
                             TR::Node          *n,
                             TR::RegisterDependencyConditions *cond,
                             TR::CodeGenerator *cg) :
                             J9::InstructionConnector(cg, op, n) { self()->initialize(NULL, false, cond, true); }


TR::Instruction::Instruction(TR::InstOpCode::Mnemonic    op,
                             TR::Node          *n,
                             TR::RegisterDependencyConditions * cond,
                             TR::Instruction   *precedingInstruction,
                             TR::CodeGenerator *cg) :
                             J9::InstructionConnector(cg, precedingInstruction, op, n) { self()->initialize(precedingInstruction, true, cond, true); }


#endif /* TR_INSTRUCTION_INCL */
