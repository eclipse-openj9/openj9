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

#include "z/codegen/ForceRecompilationSnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *
TR::S390ForceRecompilationSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;
   TR::SymbolReference * glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390induceRecompilation, false, false, false);

   // Set up the start of data constants and jump to helper.
   // We generate:
   //    LARL  r14, <Start of DC>
   //    BRCL  <Helper Addr>

   // LARL - Add Relocation the data constants to this LARL.
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getDataConstantSnippet()->getSnippetLabel()));

   *(int16_t *) cursor = 0xC0E0;
   cursor += sizeof(int16_t);

   // Place holder.  Proper offset will be calculated by relocation.
   *(int32_t *) cursor = 0xDEADBEEF;
   cursor += sizeof(int32_t);

#if !defined(PUBLIC_BUILD)
   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);
#endif

   // BRCL
   *(int16_t *) cursor = 0xC0F4;
   cursor += sizeof(int16_t);

   // Calculate the relative offset to get to helper method.
   // If MCC is not supported, everything should be reachable.
   // If MCC is supported, we will look up the appropriate trampoline, if
   //     necessary.
   intptrj_t destAddr = (intptrj_t)(glueRef->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      if (NEEDS_TRAMPOLINE(destAddr, cursor, cg()))
         {
         // Destination is beyond our reachable jump distance, we'll find the trampoline.
         destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
         this->setUsedTrampoline(true);
         }
      }
#endif

   TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);

   AOTcgDiag1(comp, "add TR_HelperAddress cursor=%x\n", cursor);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t*) glueRef, TR_HelperAddress, cg()),
                             __FILE__, __LINE__, getNode());

   cursor += sizeof(int32_t);

   return cursor;

   }


uint32_t
TR::S390ForceRecompilationSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   uint32_t length = 12;  // LARL + BRCL

#if !defined(PUBLIC_BUILD)
   length += getRuntimeInstrumentationOnOffInstructionLength(cg());
#endif

   return length;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ForceRecompilationSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Force Recompilation Call Snippet");

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "LARL \tGPR14, <%p>\t# Addr of DataConst",
                                (intptrj_t) snippet->getDataConstantSnippet()->getSnippetLabel()->getCodeLocation());
   bufferPos += 6;

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRCL \t<%p>\t\t# Branch to %s %s",
                   snippet->getSnippetDestAddr(),
                   getName(_cg->getSymRef(TR_S390induceRecompilation)),
                   snippet->usedTrampoline()?"- Trampoline Used.":"");
   bufferPos += 6;
   }

TR::S390ForceRecompilationDataSnippet::S390ForceRecompilationDataSnippet(TR::CodeGenerator *cg,
                                        TR::Node *node,
                                        TR::LabelSymbol *restartLabel):
     TR::S390ConstantDataSnippet(cg,node,generateLabelSymbol(cg),0),
     _restartLabel(restartLabel)
   {
   }

uint8_t *
TR::S390ForceRecompilationDataSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   AOTcgDiag1(comp, "TR::S390ForceRecompilationDataSnippet::emitSnippetBody cursor=%x\n", cursor);
   getSnippetLabel()->setCodeLocation(cursor);

   /** IMPORTANT **/
   // If the fields in this snippet are modified, the respective changes must be reflected
   // in jitrt.dev/s390/Recompilation.m4
   //    RetAddrInForceRecompSnippet and StartPCInForceRecompSnippet vars.

   // Return Address
   *(intptrj_t *) cursor = (intptrj_t)getRestartLabel()->getCodeLocation();
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(intptrj_t);

   // Start PC
   *(intptrj_t *) cursor = (intptrj_t)cg()->getCodeStart();
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(intptrj_t);

   return cursor;
   }

uint32_t
TR::S390ForceRecompilationDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   // Data Snippet is only 2 pointers:
   //     Return Address
   //     StartPC
   return 2 * sizeof(intptrj_t);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ForceRecompilationDataSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Force Recompilation Data Snippet");
   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Main Code Return Address", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Method Start PC (to patch)", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);
   }

