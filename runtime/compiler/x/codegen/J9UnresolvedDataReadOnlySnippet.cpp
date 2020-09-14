/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/J9UnresolvedDataReadOnlySnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Snippet.hpp"
#include "il/LabelSymbol.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"

namespace TR { class Node; }

J9::X86::UnresolvedDataReadOnlySnippet::UnresolvedDataReadOnlySnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *dataSymRef,
      intptr_t resolveDataAddress,
      TR::LabelSymbol *snippetLabel,
      TR::LabelSymbol *startResolveSequenceLabel,
      TR::LabelSymbol *volatileFenceLabel,
      TR::LabelSymbol *doneLabel,
      bool isStore) :
   Snippet(cg, node, snippetLabel),
      _resolveDataAddress(resolveDataAddress),
      _dataSymRef(dataSymRef),
      _startResolveSequenceLabel(startResolveSequenceLabel),
      _volatileFenceLabel(volatileFenceLabel),
      _doneLabel(doneLabel)
   {
   if (isStore)
      {
      setUnresolvedStore();
      }

   prepareSnippetForGCSafePoint();
   }


TR_RuntimeHelper
J9::X86::UnresolvedDataReadOnlySnippet::getHelper()
   {
   if (getDataSymbol()->isShadow())
      return isUnresolvedStore() ? TR_X86interpreterUnresolvedFieldSetterReadOnlyGlue :
         TR_X86interpreterUnresolvedFieldReadOnlyGlue;

   if (getDataSymbol()->isClassObject())
      return getDataSymbol()->addressIsCPIndexOfStatic() ? TR_X86interpreterUnresolvedClassFromStaticFieldReadOnlyGlue :
         TR_X86interpreterUnresolvedClassReadOnlyGlue;

   if (getDataSymbol()->isConstString())
      return TR_X86interpreterUnresolvedStringReadOnlyGlue;

   if (getDataSymbol()->isConstMethodType())
      return TR_interpreterUnresolvedMethodTypeReadOnlyGlue;

   if (getDataSymbol()->isConstMethodHandle())
      return TR_interpreterUnresolvedMethodHandleReadOnlyGlue;

   if (getDataSymbol()->isCallSiteTableEntry())
      return TR_interpreterUnresolvedCallSiteTableEntryReadOnlyGlue;

   if (getDataSymbol()->isMethodTypeTableEntry())
      return TR_interpreterUnresolvedMethodTypeTableEntryReadOnlyGlue;

   if (getDataSymbol()->isConstantDynamic())
      return TR_X86interpreterUnresolvedConstantDynamicReadOnlyGlue;

   // Must be a static data reference

   return isUnresolvedStore() ? TR_X86interpreterUnresolvedStaticFieldSetterReadOnlyGlue :
                         TR_X86interpreterUnresolvedStaticFieldReadOnlyGlue;
   }



uint8_t *
J9::X86::UnresolvedDataReadOnlySnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();

   uint8_t *snippetStart  = cursor;
   getSnippetLabel()->setCodeLocation(snippetStart);


   TR::SymbolReference *glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false);
   intptr_t glueAddress = (intptr_t)glueSymRef->getMethodAddress();

/*
   cg()->addProjectSpecializedRelocation(cursor+1, (uint8_t *)glueSymRef, NULL, TR_HelperAddress,
   __FILE__, __LINE__, getNode());
*/

   // Call to the glue routine
   //
   const intptr_t rip = (intptr_t)(cursor+5);
   if (cg()->needRelocationsForHelpers() || NEEDS_TRAMPOLINE(glueAddress, rip, cg()))
      {
      glueAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(glueSymRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT(IS_32BIT_RIP(glueAddress, rip), "Local helper trampoline should be reachable directly.\n");
      }

   *cursor++ = 0xe8;    // CALLImm4

   int32_t offset = (int32_t)((intptr_t)glueAddress - rip);
   *(int32_t *)cursor = offset;
   cursor += 4;

   uint8_t *helperCallRA = cursor;

   //   dd [RIP offset to resolution data] ; 32-bit (position independent).  Relative to the start of the RA
   //                                      ;    of the resolution helper call
   //
   TR_ASSERT_FATAL(IS_32BIT_RIP(_resolveDataAddress, helperCallRA), "resolve data is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(_resolveDataAddress - (intptr_t)helperCallRA);
   cursor += 4;

   //   dd Constant pool index + encoding flags
   //
   cursor = emitConstantPoolIndex(cursor);

   //   dd [RIP offset to start of mainline resolution ]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                     ;    of the resolution helper call
   //
   intptr_t startResolveSequenceLabelAddress = reinterpret_cast<intptr_t>(_startResolveSequenceLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(startResolveSequenceLabelAddress, helperCallRA), "startResolveSequenceLabel is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(startResolveSequenceLabelAddress - (intptr_t)helperCallRA);
   cursor += 4;

   if (_volatileFenceLabel)
      {
      _volatileFenceLabel->setCodeLocation(cursor);

      *cursor++ = 0x0f;  // MFENCE
      *cursor++ = 0xae;
      *cursor++ = 0xf0;

      *cursor++ = 0xe9;  // JMP done
      intptr_t doneLabelAddress = reinterpret_cast<intptr_t>(_doneLabel->getCodeLocation());
      *(int32_t*)cursor = (int32_t)(doneLabelAddress - (intptr_t)(cursor+4));
      cursor += 4;
      }

   return cursor;
   }


uint32_t
J9::X86::UnresolvedDataReadOnlySnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t length;

   length =   5    // CALL
            + 4    // resolve data RIP
            + 4    // cpIndex
            + 4;   // start of mainline resolve sequence RIP

   if (_volatileFenceLabel)
      {
      length +=   3   // MFENCE
                + 5;  // JMP
      }

   return length;
   }


uint8_t *
J9::X86::UnresolvedDataReadOnlySnippet::emitConstantPoolIndex(uint8_t *cursor)
   {
   TR::Symbol *dataSymbol = getDataSymbol();
   int32_t cpIndex;

   if (dataSymbol->isCallSiteTableEntry())
      {
      cpIndex = dataSymbol->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (dataSymbol->isMethodTypeTableEntry())
      {
      cpIndex = dataSymbol->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else // constant pool index
      {
      cpIndex = _dataSymRef->getCPIndex();
      }

   // For unresolved object refs, the snippet need not be patched because its
   // data is already correct.
   //
//   if (dataSymbol->isConstObjectRef()) cpIndex |= cpIndex_doNotPatchSnippet;

   // Identify static resolution.
   //
   if (!dataSymbol->isShadow() && !dataSymbol->isClassObject() && !dataSymbol->isConstObjectRef())
      {
      cpIndex |= cpIndex_isStaticResolution;
      }

   if (cg()->comp()->target().isSMP())
      {
      if (!dataSymbol->isClassObject() && !dataSymbol->isConstObjectRef() && isUnresolvedStore() && dataSymbol->isVolatile())
         {
         cpIndex |= cpIndex_checkVolatility;
         }
      }

   *(int32_t *)cursor = cpIndex;
   cursor += 4;

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, J9::X86::UnresolvedDataReadOnlySnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));
   }
