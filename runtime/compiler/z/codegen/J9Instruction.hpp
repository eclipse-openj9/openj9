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

#ifndef J9Z_INSTRUCTIONBASE_INCL
#define J9Z_INSTRUCTIONBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_INSTRUCTION_CONNECTOR
#define J9_INSTRUCTION_CONNECTOR
   namespace J9 { namespace Z { class Instruction; } }
   namespace J9 { typedef J9::Z::Instruction InstructionConnector; }
#else
   #error J9::Z::Instruction expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9Instruction.hpp"
#include "z/codegen/S390AOTRelocation.hpp"

namespace TR { class S390EncodingRelocation; }

namespace J9
{

namespace Z

{

class OMR_EXTENSIBLE Instruction : public J9::Instruction
   {
   public:

   Instruction(
      TR::CodeGenerator *cg,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node = 0) : J9::Instruction(cg, op, node) { _encodingRelocation = NULL; }

   Instruction(
      TR::CodeGenerator *cg,
      TR::Instruction *precedingInstruction,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node = 0) : J9::Instruction(cg, precedingInstruction, op, node) { _encodingRelocation = NULL; }

   void addEncodingRelocation(TR::CodeGenerator *codeGen, uint8_t *cursor, char* file, uintptr_t line, TR::Node* node);

   TR::S390EncodingRelocation* getEncodingRelocation() { return _encodingRelocation; }
   TR::S390EncodingRelocation* setEncodingRelocation(TR::S390EncodingRelocation *encodeRelo) { return _encodingRelocation = encodeRelo; } 

   private:

   TR::S390EncodingRelocation *_encodingRelocation;
   };

}

}

#endif /* TRJ9_INSTRUCTIONBASE_INCL */
