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

#ifndef TR_MEMORYREFERENCE_INCL
#define TR_MEMORYREFERENCE_INCL

#include "codegen/J9MemoryReference.hpp"

namespace TR { class Snippet; }

namespace TR
{

class OMR_EXTENSIBLE MemoryReference : public J9::MemoryReferenceConnector
   {
   public:

   MemoryReference(TR::CodeGenerator *cg) :
      J9::MemoryReferenceConnector(cg) {}

   MemoryReference(TR::Register *br,
      int32_t disp,
      TR::CodeGenerator *cg,
      const char *name=NULL ) :
         J9::MemoryReferenceConnector(br, disp, cg, name) {}

   MemoryReference(TR::Register *br,
      int32_t disp,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg) :
         J9::MemoryReferenceConnector(br, disp, symRef, cg) {}

   MemoryReference(TR::Register *br,
      TR::Register *ir,
      int32_t disp,
      TR::CodeGenerator *cg) :
         J9::MemoryReferenceConnector(br, ir, disp, cg) {}

   MemoryReference(int32_t disp,
      TR::CodeGenerator *cg,
      bool isConstantDataSnippet=false) :
         J9::MemoryReferenceConnector(disp, cg, isConstantDataSnippet) {}

   MemoryReference(TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg,
      bool canUseRX = false,
      TR_StorageReference *storageReference=NULL) :
         J9::MemoryReferenceConnector(rootLoadOrStore, cg, canUseRX, storageReference) {}

   MemoryReference(TR::Node *addressTree,
      bool canUseIndex,
      TR::CodeGenerator *cg) :
         J9::MemoryReferenceConnector(addressTree, canUseIndex, cg) {}

   MemoryReference(TR::Node *node,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg,
      TR_StorageReference *storageReference=NULL) :
         J9::MemoryReferenceConnector(node, symRef, cg, storageReference) {}

   MemoryReference(TR::Snippet *s,
      TR::CodeGenerator *cg,
      TR::Register* base,
      TR::Node* node) :
         J9::MemoryReferenceConnector(s, cg, base, node) {}

   MemoryReference(TR::Snippet *s,
      TR::Register* indx,
      int32_t disp,
      TR::CodeGenerator *cg) :
         J9::MemoryReferenceConnector(s, indx, disp, cg) {}

   MemoryReference(MemoryReference& mr, int32_t n, TR::CodeGenerator *cg) :
      J9::MemoryReferenceConnector(mr, n, cg) {}
   };
}

#endif
