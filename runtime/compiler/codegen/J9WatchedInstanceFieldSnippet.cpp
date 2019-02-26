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

#include "codegen/J9WatchedInstanceFieldSnippet.hpp"
#include "codegen/Relocation.hpp"

TR::J9WatchedInstanceFieldSnippet::J9WatchedInstanceFieldSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, UDATA os)
   : TR::Snippet(cg, node, generateLabelSymbol(cg), false)
   {
   instanceFieldData.method = m;
   instanceFieldData.location = loc;
   instanceFieldData.offset = os;
   }

uint8_t *TR::J9WatchedInstanceFieldSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Node *node = getNode();

   // We emit the dataSnippet based on the assumption that the J9JITWatchedInstanceFieldData structure is laid out as below:
/*   typedef struct J9JITWatchedInstanceFieldData {
         J9Method *method;               // Currently executing method
         UDATA location;                 // Bytecode PC index
         UDATA offset;                   // Field offset (not including header)
   } J9JITWatchedInstanceFieldData; */

   // Emit each field and add a relocation record (for AOT compiles) for any field if needed.

   J9JITWatchedInstanceFieldData *str = reinterpret_cast<J9JITWatchedInstanceFieldData *>(cursor);
   str->method = instanceFieldData.method;
   str->location = instanceFieldData.location;
   str->offset = instanceFieldData.offset;

   if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
      {
      cg()->addExternalRelocation(
         new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedInstanceFieldData, method), reinterpret_cast<uint8_t *>(instanceFieldData.method), reinterpret_cast<uint8_t *>(TR::SymbolType::typeMethod), TR_SymbolFromManager, cg()),
         __FILE__,
         __LINE__,
         node);
      }
   else
      {
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor  + offsetof(J9JITWatchedInstanceFieldData, method), NULL, TR_RamMethod, cg()), __FILE__, __LINE__, node);
      }
   cursor += sizeof(J9JITWatchedInstanceFieldData);

   return cursor;
   }

void TR::J9WatchedInstanceFieldSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t *bufferPos = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), bufferPos, "J9WatchedInstanceFieldSnippet");

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(J9Method *));
   trfprintf(pOutFile, "DC   \t%p \t\t# J9Method", *(reinterpret_cast<J9Method **>(bufferPos)));
   bufferPos += sizeof(J9Method *);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(UDATA));
   trfprintf(pOutFile, "DC   \t%lu \t\t# location", *(reinterpret_cast<UDATA *>(bufferPos)));
   bufferPos += sizeof(UDATA);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(UDATA));
   trfprintf(pOutFile, "DC   \t%lu \t\t# offset", *(reinterpret_cast<UDATA *>(bufferPos)));
   bufferPos += sizeof(UDATA);
   }

