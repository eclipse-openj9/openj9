/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#ifndef J9_Z_UNRESOLVEDDATA_READONLY_SNIPPET_INCL
#define J9_Z_UNRESOLVEDDATA_READONLY_SNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>
#include "il/SymbolReference.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Symbol; }

namespace J9
{

namespace Z
{

class UnresolvedDataReadOnlySnippet : public TR::Snippet
   {
   public:

   struct CCUnresolvedData
      {
      intptr_t indexOrAddress;
      intptr_t cpAddress;
      intptr_t isVolatile;
      };

   public:

   UnresolvedDataReadOnlySnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *dataSymRef,
      intptr_t resolveDataAddress,
      TR::LabelSymbol *startResolveSequenceLabel,
      TR::LabelSymbol *volatileFenceLabel,
      TR::LabelSymbol *doneLabel);

   virtual Kind getKind() { return IsUnresolvedDataReadOnly; }

   TR::SymbolReference *getDataSymbolReference() { return dataSymRef; }

   CCUnresolvedData *getCCUnresolvedData() { return reinterpret_cast<CCUnresolvedData*>(resolveDataAddress); }

   TR::LabelSymbol *getVolatileFenceLabel() { return volatileFenceLabel; }
   TR::LabelSymbol *getDoneLabel() { return doneLabel; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   virtual void print(TR::FILE *f, TR_Debug *debug);

   private:

   TR_RuntimeHelper getHelper();

   int32_t getCPIndex();

   private:

   TR::SymbolReference *dataSymRef;

   TR::LabelSymbol *startResolveSequenceLabel;
   TR::LabelSymbol *volatileFenceLabel;
   TR::LabelSymbol *doneLabel;

   intptr_t resolveDataAddress;
   };

}

}

#endif
