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

#include "x/codegen/RecompilationSnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/CodeCacheManager.hpp"

TR::X86RecompilationSnippet::X86RecompilationSnippet(TR::LabelSymbol    *lab,
                                                         TR::Node          *node,
                                                         TR::CodeGenerator *cg)
   : TR::Snippet(cg, node, lab, true)
   {
   setDestination(cg->symRefTab()->findOrCreateRuntimeHelper(TR::Compiler->target.is64Bit()? TR_AMD64countingRecompileMethod : TR_IA32countingRecompileMethod, false, false, false));
   }

uint32_t TR::X86RecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {

   return 9;
   }
void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RecompilationSnippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   TR::MethodSymbol *recompileSym  = snippet->getDestination()->getSymbol()->castToMethodSymbol();

   uint8_t         *bufferPos        = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(snippet->getDestination()));

   printPrefix(pOutFile, NULL, bufferPos, 5);
   trfprintf(pOutFile, "call\t%s \t\t%s Helper Address = " POINTER_PRINTF_FORMAT,
                 getName(snippet->getDestination()),
                 commentString(),
                 recompileSym->getMethodAddress());
   bufferPos += 5;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "%s  \t%s%08x%s\t\t%s Offset to startPC",
                 ddString(),
                 hexPrefixString(),
                 _cg->getCodeStart() - bufferPos,
                 hexSuffixString(),
                 commentString());
   bufferPos += 4;
   }



uint8_t *TR::X86RecompilationSnippet::emitSnippetBody()
   {

   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   intptrj_t helperAddress = (intptrj_t)_destination->getMethodAddress();
   *buffer++ = 0xe8; // CallImm4
   if (NEEDS_TRAMPOLINE(helperAddress, buffer+4, cg()))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(_destination->getReferenceNumber(), (void *)buffer);
      TR_ASSERT(IS_32BIT_RIP(helperAddress, buffer+4), "Local helper trampoline should be reachable directly.\n");
      }
   *(int32_t *)buffer = ((uint8_t*)helperAddress - buffer) - 4;
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,
                                                         (uint8_t *)_destination,
                                                         TR_HelperAddress, cg()),
                                                         __FILE__, __LINE__, getNode());
   buffer += 4;

   uint8_t *bufferBase = buffer;

   // Offset to the start of the method.  This is the instruction that must be
   // patched when the new code is built.
   //
   *(uint32_t *)buffer = (uint32_t)(cg()->getCodeStart() - bufferBase);
   buffer += 4;

   return buffer;
   }

