/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef PPC_FORCERECOMPILATIONSNIPPET_INCL
#define PPC_FORCERECOMPILATIONSNIPPET_INCL

#include "codegen/Snippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }

namespace TR {

class PPCForceRecompilationSnippet : public TR::Snippet
   {
   TR::Instruction *branchToSnippet;
   TR::LabelSymbol *_doneLabel;
   public:

   PPCForceRecompilationSnippet(
         TR::LabelSymbol *snippetlab,
         TR::LabelSymbol *doneLab,
         TR::Instruction *bts,
         TR::CodeGenerator *cg)
      : _doneLabel(doneLab), TR::Snippet(cg, 0, snippetlab, false), branchToSnippet(bts)
      {
      }

   virtual Kind getKind() { return IsForceRecompilation; }

   TR::Instruction *getBranchToSnippet() {return branchToSnippet;}

   TR::LabelSymbol *getDoneLabel() {return _doneLabel;}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif
