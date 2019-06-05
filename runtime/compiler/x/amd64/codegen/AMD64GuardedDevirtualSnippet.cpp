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

#include "x/amd64/codegen/AMD64GuardedDevirtualSnippet.hpp"

#include <stddef.h>
#include "codegen/GuardedDevirtualSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"

uint8_t *TR::AMD64GuardedDevirtualSnippet::loadArgumentsIfNecessary(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea)
   {
   if (!_realMethodSymbolReference)
      return cursor;

   TR::MethodSymbol    *methodSymbol = _realMethodSymbolReference->getSymbol()->castToMethodSymbol();
   if (isLoadArgumentsNecessary(methodSymbol))
      {
      // Devirtualized VMInternalNatives have their args evaluated to the
      // stack, so we must load them into regs before the vft dispatch
      TR::Linkage *linkage = cg()->getLinkage(methodSymbol->getLinkageConvention());
      cursor = linkage->loadArguments(callNode, cursor, calculateSizeOnly, sizeOfFlushArea, false);
      }
   return cursor;
   }

uint8_t *TR::AMD64GuardedDevirtualSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   uint8_t *cursor = loadArgumentsIfNecessary(getNode(), buffer, false, NULL);
   cg()->setBinaryBufferCursor(cursor);
   cursor = TR::X86GuardedDevirtualSnippet::emitSnippetBody();
   getSnippetLabel()->setCodeLocation(buffer);
   cg()->setBinaryBufferCursor(buffer);
   return cursor;
   }

uint32_t TR::AMD64GuardedDevirtualSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t sizeOfFlushArea = 0;
   loadArgumentsIfNecessary(getNode(), (uint8_t*)0 + estimatedSnippetStart, true, &sizeOfFlushArea);
   return sizeOfFlushArea + TR::X86GuardedDevirtualSnippet::getLength(estimatedSnippetStart + sizeOfFlushArea);
   }

bool TR::AMD64GuardedDevirtualSnippet::isLoadArgumentsNecessary(TR::MethodSymbol *methodSymbol)
   {
   return (methodSymbol && methodSymbol->isVMInternalNative());
   }
