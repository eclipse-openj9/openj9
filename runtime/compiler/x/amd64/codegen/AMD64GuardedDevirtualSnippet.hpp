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

#ifndef AMD64GUARDEDDEVIRTUALSNIPPET_INCL
#define AMD64GUARDEDDEVIRTUALSNIPPET_INCL

#include "x/codegen/GuardedDevirtualSnippet.hpp"

#include <stdint.h>
#include "il/SymbolReference.hpp"

namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class MethodSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }

namespace TR {

class AMD64GuardedDevirtualSnippet : public TR::X86GuardedDevirtualSnippet
   {
   int32_t _argSize;

   uint8_t *loadArgumentsIfNecessary(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfArgumentFlushArea);

   private:
   TR::SymbolReference * _realMethodSymbolReference;

   public:

   AMD64GuardedDevirtualSnippet(TR::CodeGenerator   *cg,
                                TR::Node            *node,
                                TR::SymbolReference *realMethodSymRef,
                                TR::LabelSymbol      *restartlab,
                                TR::LabelSymbol      *snippetlab,
                                int32_t             vftoffset,
                                TR::Block           *currentBlock,
                                TR::Register        *classRegister,
                                int32_t             argSize):
      TR::X86GuardedDevirtualSnippet(cg, node, restartlab, snippetlab, vftoffset, currentBlock, classRegister),
      _argSize(argSize), _realMethodSymbolReference(realMethodSymRef) {}

   bool isLoadArgumentsNecessary(TR::MethodSymbol *methodSymbol);
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual TR::SymbolReference* getRealMethodSymbolReference() { return _realMethodSymbolReference; }

   };

}

#endif

