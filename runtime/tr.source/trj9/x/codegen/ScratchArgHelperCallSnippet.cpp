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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "x/codegen/ScratchArgHelperCallSnippet.hpp"

#include <stdint.h>
#include "env/IO.hpp"
#include "env/jittypes.h"

inline bool is32Bit(uintptrj_t value)
   {
   if (sizeof(value) <= 4) // Work around C++ and/or gcc weirdness where value >> sizeof(value)/8 == value
      return true;
   else
      return ((uint64_t)value >> 32) == 0;
   }

uint8_t *TR::X86ScratchArgHelperCallSnippet::genHelperCall(uint8_t *buffer)
   {
   if (is32Bit(_scratchArg))
      {
      // mov edi, _scratchArg
      *buffer++ = 0xbf;
      *(uint32_t*)buffer = _scratchArg;
      buffer += 4;
      }
   else
      {
      // mov rdi, _scratchArg
      *buffer++ = 0x48;
      *buffer++ = 0xbf;
      *(uint64_t*)buffer = _scratchArg;
      buffer += 8;
      }
   return TR::X86HelperCallSnippet::genHelperCall(buffer);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ScratchArgHelperCallSnippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t   *bufferPos  = snippet->getSnippetLabel()->getCodeLocation();
   uintptrj_t scratchArg = snippet->getScratchArg();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(snippet->getDestination()));

   // Print the argument load
   //
   if (sizeof(scratchArg) <= 4 || ((uint64_t)scratchArg) >> 32 == 0)
      {
      printPrefix(pOutFile, NULL, bufferPos, 5);
      trfprintf(pOutFile, "mov \tedi, " POINTER_PRINTF_FORMAT "\t\t%s Load argument into scratch reg", scratchArg,
                    commentString());
      bufferPos += 5;
      }
   else
      {
      printPrefix(pOutFile, NULL, bufferPos, 10);
      trfprintf(pOutFile, "mov \trdi, " POINTER_PRINTF_FORMAT "\t%s Load argument into scratch reg", scratchArg,
                    commentString());
      bufferPos += 10;
      }

   // Print the rest of the snippet
   //
   printBody(pOutFile, (TR::X86HelperCallSnippet*)snippet, bufferPos);
   }


uint32_t TR::X86ScratchArgHelperCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t movSize = is32Bit(_scratchArg)? 5 : 10;
   return movSize + TR::X86HelperCallSnippet::getLength(movSize + estimatedSnippetStart);
   }

uint8_t *TR::X86StackOverflowCheckSnippet::genHelperCall(uint8_t *buffer)
   {
   // This code assumes that the superclass' call to genHelperCall() will have
   // the buffer pointing to the instruction immediately following the helper
   // call.
   //
   uint8_t *cursor = TR::X86ScratchArgHelperCallSnippet::genHelperCall(buffer);
   return cursor;
   }
