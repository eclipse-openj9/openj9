/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef J9_ARM64_UNRESOLVEDDATASNIPPET_INCL
#define J9_ARM64_UNRESOLVEDDATASNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_UNRESOLVEDDATASNIPPET_CONNECTOR
#define J9_UNRESOLVEDDATASNIPPET_CONNECTOR
namespace J9 { namespace ARM64 { class UnresolvedDataSnippet; } }
namespace J9 { typedef J9::ARM64::UnresolvedDataSnippet UnresolvedDataSnippetConnector; }
#else
#error J9::ARM64::UnresolvedDataSnippet expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9UnresolvedDataSnippet.hpp"

#include "il/SymbolReference.hpp"

namespace TR { class MemoryReference; }
namespace TR { class Symbol; }

namespace J9
{

namespace ARM64
{

class UnresolvedDataSnippet : public J9::UnresolvedDataSnippet
   {

   TR::MemoryReference  *_memoryReference;

   public:

   /**
    * @brief Constructor
    */
   UnresolvedDataSnippet(TR::CodeGenerator *cg, TR::Node *node, TR::SymbolReference *symRef, bool isStore, bool isGCSafePoint) :
      J9::UnresolvedDataSnippet(cg, node, symRef, isStore, isGCSafePoint),
      _memoryReference(NULL)
      {
      }

   /**
    * @brief Answers the Snippet kind
    * @return Snippet kind
    */
   virtual Kind getKind() { return IsUnresolvedData; }

   /**
    * @brief Answers the DataSymbol
    * @return DataSymbol
    */
   TR::Symbol *getDataSymbol() { return getDataSymbolReference()->getSymbol(); }

   /**
    * @brief Answers the MemoryReference
    * @return MemoryReference
    */
   TR::MemoryReference *getMemoryReference() { return _memoryReference; }
   /**
    * @brief Sets the MemoryReference
    * @return MemoryReference
    */
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *mr) { return (_memoryReference = mr); }

   /**
    * @brief Emits the Snippet body
    * @return instruction cursor
    */
   virtual uint8_t *emitSnippetBody();

   /**
    * @brief Answers the Snippet length
    * @return Snippet length
    */
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

}

#endif
