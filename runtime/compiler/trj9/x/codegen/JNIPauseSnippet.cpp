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

#include "x/codegen/JNIPauseSnippet.hpp"
#include "env/IO.hpp"

uint8_t *TR::X86JNIPauseSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   *buffer++ = 0xf3;  // PAUSE
   *buffer++ = 0x90;

   return genRestartJump(buffer);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86JNIPauseSnippet  * snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   printPrefix(pOutFile, NULL, bufferPos, 2);
   trfprintf(pOutFile, "pause\t\t%s spin loop pause",
                 commentString());
   bufferPos += 2;

   printRestartJump(pOutFile, snippet, bufferPos);
   }



uint32_t TR::X86JNIPauseSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 2 + estimateRestartJumpLength(estimatedSnippetStart + 2);
   }
