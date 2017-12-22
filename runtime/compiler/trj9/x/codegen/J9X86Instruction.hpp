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

#ifndef J9_X86INSTRUCTION_INCL
#define J9_X86INSTRUCTION_INCL

#include "codegen/OMRX86Instruction.hpp"

namespace TR { class Node; }

namespace TR { class MemoryReference; }
namespace TR { class CodeGenerator; }
namespace TR { class Snippet; }
namespace TR { class UnresolvedDataSnippet; }

namespace TR {

class X86MemImmSnippetInstruction : public TR::X86MemImmInstruction
   {
   TR::UnresolvedDataSnippet *_unresolvedSnippet;

   public:

   X86MemImmSnippetInstruction(TR_X86OpCodes op,
                               TR::Node *node,
                               TR::MemoryReference *mr,
                               int32_t imm,
                               TR::UnresolvedDataSnippet *us,
                               TR::CodeGenerator *cg);

   X86MemImmSnippetInstruction(TR::Instruction *precedingInstruction,
                               TR_X86OpCodes op,
                               TR::MemoryReference *mr,
                               int32_t imm,
                               TR::UnresolvedDataSnippet *us,
                               TR::CodeGenerator *cg);

   virtual char *description() { return "X86MemImmSnippet"; }

   virtual Kind getKind() { return IsMemImmSnippet; }

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *us)
      {
      return (_unresolvedSnippet = us);
      }

   virtual TR::Snippet *getSnippetForGC();
   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t *generateBinaryEncoding();

   };


class X86CheckAsyncMessagesMemRegInstruction : public TR::X86MemRegInstruction
   {
   public:

   X86CheckAsyncMessagesMemRegInstruction(TR::Node *node, TR_X86OpCodes op, TR::MemoryReference *mr, TR::Register *valueReg, TR::CodeGenerator *cg);

   virtual char *description() { return "X86CheckAsyncMessagesMemReg"; }

   virtual uint8_t *generateBinaryEncoding();

   };


class X86CheckAsyncMessagesMemImmInstruction : public TR::X86MemImmInstruction
   {
   public:

   X86CheckAsyncMessagesMemImmInstruction(TR::Node *node, TR_X86OpCodes op, TR::MemoryReference *mr, int32_t value, TR::CodeGenerator *cg);

   virtual char *description() { return "X86CheckAsyncMessagesMemImm"; }

   virtual uint8_t *generateBinaryEncoding();

   };


class X86StackOverflowCheckInstruction : public TR::X86RegMemInstruction
   {
   public:

   X86StackOverflowCheckInstruction(
      TR::Instruction        *precedingInstruction,
      TR_X86OpCodes          op,
      TR::Register           *cmpRegister,
      TR::MemoryReference *mr,
      TR::CodeGenerator      *cg);

   virtual char *description() { return "X86StackOverflowCheck"; }

   virtual uint8_t *generateBinaryEncoding();

   };

}


TR::X86MemImmSnippetInstruction * generateMemImmSnippetInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference *mr, int32_t imm, TR::UnresolvedDataSnippet *, TR::CodeGenerator *cg);

TR::X86CheckAsyncMessagesMemImmInstruction *generateCheckAsyncMessagesInstruction(
   TR::Node               *node,
   TR_X86OpCodes          op,
   TR::MemoryReference *mr,
   int32_t                value,
   TR::CodeGenerator      *cg);

TR::X86CheckAsyncMessagesMemRegInstruction *generateCheckAsyncMessagesInstruction(
   TR::Node               *node,
   TR_X86OpCodes          op,
   TR::MemoryReference *mr,
   TR::Register           *reg,
   TR::CodeGenerator      *cg);

TR::X86StackOverflowCheckInstruction *generateStackOverflowCheckInstruction(
   TR::Instruction        *precedingInstruction,
   TR_X86OpCodes          op,
   TR::Register           *cmpRegister,
   TR::MemoryReference *mr,
   TR::CodeGenerator      *cg);

#endif
