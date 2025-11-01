/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "x/codegen/ForceRecompilationSnippet.hpp"

#include "codegen/Relocation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/OMRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "x/codegen/X86Recompilation.hpp"

// TODO: Combine this with TR::X86RecompilationSnippet
// TODO: This class should be named TR::X86InduceRecompilationSnippet

uint8_t *TR::X86ForceRecompilationSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   TR::SymbolReference *helper = cg()->symRefTab()->findOrCreateRuntimeHelper(cg()->comp()->target().is64Bit()? TR_AMD64induceRecompilation : TR_IA32induceRecompilation);
   intptr_t helperAddress = (intptr_t)helper->getMethodAddress();
   *buffer = 0xe8; // CallImm4
   if (cg()->directCallRequiresTrampoline(helperAddress, reinterpret_cast<intptr_t>(buffer++)))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(helper->getReferenceNumber(), (void *)buffer);
      TR_ASSERT(IS_32BIT_RIP(helperAddress, buffer+4), "Local helper trampoline should be reachable directly.\n");
      }
   *(int32_t *)buffer = ((uint8_t*)helperAddress - buffer) - 4;

   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         buffer,
         (uint8_t *)helper,
         TR_HelperAddress, cg()),
      __FILE__,
      __LINE__,
      getNode());

   buffer += 4;

   uint8_t *bufferBase = buffer;

   buffer = genRestartJump(buffer);

   // Offset to the startPC.  We don't store the startPC itself, because on
   // AMD64 that would require 4 more bytes.
   //
   *(uint32_t *)buffer = (uint32_t)(cg()->getCodeStart() - bufferBase);
   buffer += 4;

   return buffer;
   }


void
TR_Debug::print(OMR::Logger *log, TR::X86ForceRecompilationSnippet *snippet)
   {
   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(log, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::SymbolReference *helper = _cg->getSymRef(_comp->target().is64Bit()? TR_AMD64induceRecompilation : TR_IA32induceRecompilation);
   intptr_t helperAddress = (intptr_t)helper->getMethodAddress();

   printPrefix(log, NULL, bufferPos, 5);
   log->printf("call\t%s \t%s Helper Address = " POINTER_PRINTF_FORMAT,
                 getName(helper),
                 commentString(),
                 helperAddress);
   bufferPos += 5;

   uint8_t *offsetBase = bufferPos;

   printPrefix(log, NULL, bufferPos, 5);
   printLabelInstruction(log, "jmp", snippet->getRestartLabel());
   bufferPos += 5;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("%s  \t%s%08x%s\t\t%s Offset to startPC",
                 ddString(),
                 hexPrefixString(),
                 _cg->getCodeStart() - offsetBase,
                 hexSuffixString(),
                 commentString());
   bufferPos += 4;
   }


uint32_t TR::X86ForceRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 14;
   }
