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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "p/codegen/PPCRecompilationSnippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
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
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCRecompilation.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *TR::PPCRecompilationSnippet::emitSnippetBody()
   {
   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();
   TR::SymbolReference  *countingRecompMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCcountingRecompileMethod);
   bool                 longPrologue = getBranchToSnippet()->getFarRelocation();

   getSnippetLabel()->setCodeLocation(buffer);

   intptr_t helperAddress = (intptr_t)countingRecompMethodSymRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptr_t)buffer))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(countingRecompMethodSymRef->getReferenceNumber(), (void *)buffer);
      TR_ASSERT_FATAL(comp->target().cpu.isTargetWithinIFormBranchRange(helperAddress, (intptr_t)buffer), "Helper address is out of range");
      }

   // bl distance
   *(int32_t *)buffer = 0x48000001 | ((helperAddress - (intptr_t)buffer) & 0x03ffffff);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,(uint8_t *)countingRecompMethodSymRef,TR_HelperAddress, cg()),
                          __FILE__,
                          __LINE__,
                          getNode());

   buffer += 4;

   // bodyinfo
   uintptr_t valueBits = (uintptr_t)comp->getRecompilationInfo()->getJittedBodyInfo();
   *(uintptr_t *)buffer = valueBits;

   buffer += sizeof(uintptr_t);

   // startPC
   valueBits = (uintptr_t)cg()->getCodeStart() | (longPrologue?1:0);
   *(uintptr_t *)buffer = valueBits;

   return buffer + sizeof(uintptr_t);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCRecompilationSnippet * snippet)
   {
   uint8_t             *cursor        = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Counting Recompilation Snippet");

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(_cg->getSymRef(TR_PPCcountingRecompileMethod), cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptr_t)cursor + distance, info);
   cursor += 4;

   // methodInfo
   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t;%s", _comp->getRecompilationInfo()->getMethodInfo(), "methodInfo");
   cursor += 4;

   // startPC
   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; startPC | longPrologue", _cg->getCodeStart());
   }


uint32_t TR::PPCRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return(cg()->comp()->target().is64Bit()? 20 : 12);
   }
