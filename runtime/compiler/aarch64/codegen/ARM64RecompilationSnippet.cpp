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

#include "codegen/ARM64RecompilationSnippet.hpp"

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"

uint8_t *TR::ARM64RecompilationSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *countingRecompMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64countingRecompileMethod, false, false, false);

   getSnippetLabel()->setCodeLocation(buffer);

   // bl symref
   *(int32_t *)buffer = cg()->encodeHelperBranchAndLink(countingRecompMethodSymRef, buffer, getNode());
   buffer += ARM64_INSTRUCTION_LENGTH;

   // bodyinfo
   *(intptr_t *)buffer = (intptr_t)cg()->comp()->getRecompilationInfo()->getJittedBodyInfo();
   buffer += sizeof(intptr_t);

   // startPC
   *(intptr_t *)buffer = (intptr_t)cg()->getCodeStart();
   buffer += sizeof(intptr_t);

   return buffer;
   }

uint32_t TR::ARM64RecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return ARM64_INSTRUCTION_LENGTH + sizeof(intptr_t) + sizeof(intptr_t);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64RecompilationSnippet *snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, getName(snippet));

   int32_t distance;
   distance = *((int32_t *)cursor) & 0x03ffffff; // imm26
   distance = (distance << 6) >> 4; // sign extend and add two 0 bits

   printPrefix(pOutFile, NULL, cursor, ARM64_INSTRUCTION_LENGTH);
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t; %s",
             (intptr_t)cursor + distance, getRuntimeHelperName(TR_ARM64countingRecompileMethod));
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; BodyInfo", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; startPC", *(intptr_t *)cursor);
   }
