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

#ifndef J9_ZUNRESOLVEDDATASNIPPET_INCL
#define J9_ZUNRESOLVEDDATASNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_UNRESOLVEDDATASNIPPET_CONNECTOR
#define J9_UNRESOLVEDDATASNIPPET_CONNECTOR
namespace J9 { namespace Z { class UnresolvedDataSnippet; } }
namespace J9 { typedef J9::Z::UnresolvedDataSnippet UnresolvedDataSnippetConnector; }
#else
#error J9::Z::UnresolvedDataSnippet expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9UnresolvedDataSnippet.hpp"

#include <stdint.h>
#include "codegen/Snippet.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class S390WritableDataSnippet; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Symbol; }

namespace J9
{

namespace Z
{

class UnresolvedDataSnippet : public J9::UnresolvedDataSnippet
   {
   /** _branchInstruction is actually the instruction which branch to the UDS. */
   TR::Instruction     *_branchInstruction;
   TR::SymbolReference     *_dataSymbolReference;
   TR::MemoryReference *_memoryReference;
   bool                    _isStore;

   /**
    *_dataReferenceInstruction is the instruction
    * that references it. The address of this will be resolved
    * when emitting the snippet code.
    */
   TR::Instruction         *_dataReferenceInstruction;
   TR::S390WritableDataSnippet *_unresolvedData;
   uint8_t                    *_literalPoolPatchAddress;
   uint8_t                    *_literalPoolSlot;         ///< For trace file generation

   public:

   UnresolvedDataSnippet(TR::CodeGenerator *, TR::Node *, TR::SymbolReference *s, bool isStore, bool canCauseGC);

   virtual Kind getKind() { return IsUnresolvedData; }

   TR::Instruction *getBranchInstruction() {return _branchInstruction;}
   TR::Instruction *setBranchInstruction(TR::Instruction *i)
      {
      return _branchInstruction = i;
      }

   TR::Instruction *getDataReferenceInstruction() {return _dataReferenceInstruction;}

   TR::Instruction *setDataReferenceInstruction(TR::Instruction *i);

   TR::SymbolReference *getDataSymbolReference() {return _dataSymbolReference;}

   /** Create Unresolved Data in writable data area */
   TR::S390WritableDataSnippet *createUnresolvedData(TR::CodeGenerator *cg, TR::Node * n);

   TR::S390WritableDataSnippet *getUnresolvedData()
      {
      return _unresolvedData;
      }

   uint8_t *getLiteralPoolPatchAddress() { return _literalPoolPatchAddress;}
   uint8_t *setLiteralPoolPatchAddress(uint8_t *cursor)
      {
      return _literalPoolPatchAddress = cursor;
      }
   uint8_t *getLiteralPoolSlot() { return _literalPoolSlot;}
   uint8_t *setLiteralPoolSlot(uint8_t *cursor)
      {
      return _literalPoolSlot = cursor;
      }

   TR::Symbol *getDataSymbol() {return _dataSymbolReference->getSymbol();}
   TR::MemoryReference *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *mr)
      {return _memoryReference = mr;}

   bool isInstanceData();

   bool    resolveForStore() {return _isStore;}
   virtual uint8_t *getAddressOfDataReference();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

}

#endif
