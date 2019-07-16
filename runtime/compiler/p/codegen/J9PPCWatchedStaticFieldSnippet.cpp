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

#include "codegen/J9PPCWatchedStaticFieldSnippet.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "codegen/Relocation.hpp"


uint8_t* TR::J9PPCWatchedStaticFieldSnippet::emitSnippetBody()
   {

   if (cg()->comp()->compileRelocatableCode() && !cg()->comp()->getOption(TR_UseSymbolValidationManager))
      {
      TR_ASSERT_FATAL(false, "Field Watch and AOT are not supported.");
      }
   
   // Call Base class's emitSnippetBody() to populate the snippet
   // Here Binary Buffer will be ahead by the size of the struct J9JITWatchedStaticFieldData
   uint8_t *snippetLocation = (TR::J9WatchedStaticFieldSnippet::emitSnippetBody() - TR::J9WatchedStaticFieldSnippet::getLength(0));

   // Insert the Snippet Address into TOC or patch the materalisation instructions
   // Insert into TOC 
   if (TR::Compiler->target.is64Bit() && getTOCOffset() != PTOC_FULL_INDEX)
      {
      TR_PPCTableOfConstants::setTOCSlot(getTOCOffset(), reinterpret_cast<uintptrj_t>(snippetLocation));
      }
   else if (getLowerInstruction() != NULL)
      {
      // Handle Nibbles - Generation of instructions to materialise address
      int32_t *patchAddr = reinterpret_cast<int32_t *>(getLowerInstruction()->getBinaryEncoding());
      intptrj_t addrValue = reinterpret_cast<intptrj_t>(snippetLocation);

      if (TR::Compiler->target.is64Bit())
         {
         *patchAddr |= LO_VALUE(addrValue) & 0x0000ffff;
         addrValue = cg()->hiValue(addrValue);
         *(patchAddr - 2) |= addrValue & 0x0000ffff;
         *(patchAddr - 3) |= (addrValue>>16) & 0x0000ffff;
         *(patchAddr - 4) |= (addrValue>>32) & 0x0000ffff;
         }
      else
         {
      	int32_t *patchAddress1 = reinterpret_cast<int32_t *>(getUpperInstruction()->getBinaryEncoding());
         *patchAddress1 |= cg()->hiValue(static_cast<int32_t>(reinterpret_cast<intptrj_t>(snippetLocation))) & 0x0000ffff;
         int32_t *patchAddress2 = reinterpret_cast<int32_t *>(getLowerInstruction()->getBinaryEncoding());
         *patchAddress2 |= static_cast<int32_t>(reinterpret_cast<intptrj_t>(snippetLocation)) & 0x0000ffff;
         }
      }

   // Restore cursor before returning
   return (snippetLocation + TR::J9WatchedStaticFieldSnippet::getLength(0));
   }
