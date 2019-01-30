/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef GUARDEDDEVIRTUALSNIPPET_INCL
#define GUARDEDDEVIRTUALSNIPPET_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/Snippet.hpp"
#include "x/codegen/RestartSnippet.hpp"

namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class MethodSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class SymbolReference; }

namespace TR {

class X86GuardedDevirtualSnippet  : public TR::X86RestartSnippet
   {
   TR::Block    *_currentBlock;
   TR::Register *_classObjectRegister;
   int32_t      _vtableOffset;

   public:

   X86GuardedDevirtualSnippet(TR::CodeGenerator * cg,
                              TR::Node *          node,
                              TR::LabelSymbol     *restartlab,
                              TR::LabelSymbol     *snippetlab,
                              int32_t            vftoffset,
                              TR::Block          *currentBlock,
                              TR::Register       *classRegister);

   virtual Kind getKind() { return IsGuardedDevirtual; }

   int32_t getVTableOffset()             {return _vtableOffset;}
   int32_t setVTableOffset(int32_t vfto) {return (_vtableOffset = vfto);}

   TR::Register *getClassObjectRegister()                 {return _classObjectRegister;}
   TR::Register *setClassObjectRegister(TR::Register *reg) {return (_classObjectRegister = reg);}
   
   virtual bool isLoadArgumentsNecessary(TR::MethodSymbol *methodSymbol) {return false;}
   
   // Only implemented by AMD64GuardedDevirtualSnippet
   virtual TR::SymbolReference* getRealMethodSymbolReference() {return NULL;}

   virtual TR::X86GuardedDevirtualSnippet  *getGuardedDevirtualSnippet();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   };

}

#endif
