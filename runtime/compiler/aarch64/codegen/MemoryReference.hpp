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

#ifndef TR_MEMORYREFERENCE_INCL
#define TR_MEMORYREFERENCE_INCL

#include "codegen/J9MemoryReference.hpp"

namespace TR { class Snippet; }

namespace TR
{

class OMR_EXTENSIBLE MemoryReference : public J9::MemoryReferenceConnector
   {
   public:

   MemoryReference(TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(cg) {}

   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(br, ir, cg) {}

   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         uint8_t scale,
         TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(br, ir, scale, cg) {}

   MemoryReference(
         TR::Register *br,
         int32_t disp,
         TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(br, disp, cg) {}

   MemoryReference(
         TR::Node *node,
         uint32_t len,
         TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(node, len, cg) {}

   MemoryReference(
         TR::Node *node,
         TR::SymbolReference *symRef,
         uint32_t len,
         TR::CodeGenerator *cg)
      : J9::MemoryReferenceConnector(node, symRef, len, cg) {}
   };

} // TR

#endif
