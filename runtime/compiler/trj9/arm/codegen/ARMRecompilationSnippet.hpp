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

#ifndef ARMRECOMPILATIONSNIPPET_INCL
#define ARMRECOMPILATIONSNIPPET_INCL

#include "codegen/Snippet.hpp"
//#include "codegen/ARMLinkage.hpp"
#include "codegen/Linkage.hpp"
//#include "codegen/ARMMachine.hpp"
#include "codegen/Machine.hpp"
#include "codegen/ARMInstruction.hpp"

namespace TR {

class ARMRecompilationSnippet : public TR::Snippet
   {
   TR::Instruction *branchToSnippet;
   public:

   ARMRecompilationSnippet(TR::LabelSymbol *snippetlab, TR::Instruction *bts, TR::CodeGenerator *cg)
      : TR::Snippet(cg, 0, snippetlab, false), branchToSnippet(bts)
      {
      }

   virtual Kind getKind() { return IsRecompilation; }

   TR::Instruction *getBranchToSnippet() {return branchToSnippet;}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

/*
class ARMEDORecompilationSnippet : public TR::Snippet
   {
   TR_Instruction *branchToSnippet;
   TR::LabelSymbol *_doneLabel;
   public:

   ARMEDORecompilationSnippet(TR::LabelSymbol *snippetlab,  TR::LabelSymbol *doneLab, TR_Instruction *bts, TR::CodeGenerator *cg)
      : _doneLabel(doneLab) , TR::Snippet(cg, 0, snippetlab, false), branchToSnippet(bts)
      {
      }

   virtual Kind getKind() { return IsEDORecompilation; }

   TR_Instruction *getBranchToSnippet() {return branchToSnippet;}

   TR::LabelSymbol *getDoneLabel() {return _doneLabel;}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };
*/

}

#endif
