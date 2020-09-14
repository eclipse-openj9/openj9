/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_X86_UNRESOLVEDDATA_READONLY_SNIPPET_INCL
#define J9_X86_UNRESOLVEDDATA_READONLY_SNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "il/SymbolReference.hpp"
#include "infra/Flags.hpp"
#include "runtime/Runtime.hpp"

namespace TR { class SymbolReference; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Symbol; }

namespace J9
{

namespace X86
{

class UnresolvedDataReadOnlySnippet : public TR::Snippet
   {

public:

   UnresolvedDataReadOnlySnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *dataSymRef,
      intptr_t resolveDataAddress,
      TR::LabelSymbol *snippetLabel,
      TR::LabelSymbol *startResolveSequenceLabel,
      TR::LabelSymbol *volatileFenceLabel,
      TR::LabelSymbol *doneLabel,
      bool isStore);

   Kind getKind() { return IsUnresolvedDataReadOnly; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   TR::Symbol *getDataSymbol() { return _dataSymRef->getSymbol(); }

   TR_RuntimeHelper getHelper();

   bool isUnresolvedStore()    { return _flags.testAll(TO_MASK32(IsUnresolvedStore)); }
   void setUnresolvedStore()   { _flags.set(TO_MASK32(IsUnresolvedStore)); }
   void resetUnresolvedStore() { _flags.reset(TO_MASK32(IsUnresolvedStore)); }

   uint8_t *emitConstantPoolIndex(uint8_t *cursor);

   /*
    * Flags to be passed to the resolution runtime.  These flags piggyback on
    * the unused bits of the cpIndex.
    */
   enum
      {
      cpIndex_isStaticResolution     = 0x20000000,
      cpIndex_checkVolatility        = 0x00080000,
      };

protected:

   enum
      {
      IsUnresolvedStore = TR::Snippet::NextSnippetFlag,
      NextSnippetFlag
      };

   static_assert((int32_t)NextSnippetFlag <= (int32_t)TR::Snippet::MaxSnippetFlag,
      "J9::X86::UnresolvedDataReadOnlySnippet too many flag bits for flag width");

   TR::SymbolReference *_dataSymRef;

   TR::LabelSymbol *_startResolveSequenceLabel;
   TR::LabelSymbol *_volatileFenceLabel;
   TR::LabelSymbol *_doneLabel;

   intptr_t _resolveDataAddress;

   };

}

}

#endif
