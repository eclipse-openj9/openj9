/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "x/codegen/CallSnippet.hpp"

#include "codegen/AMD64CallSnippet.hpp"
#include "codegen/AMD64PrivateLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "codegen/X86PrivateLinkage.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "objectfmt/GlobalFunctionCallData.hpp"
#include "objectfmt/ObjectFormat.hpp"

uint32_t TR::X86ResolveVirtualDispatchReadOnlyDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   // call + dd (4) + dd (4) + dd (4)
   //
   return cg()->getObjFmt()->globalFunctionCallBinaryLength() + 4 + 4 + 4;
   }


uint8_t *TR::X86ResolveVirtualDispatchReadOnlyDataSnippet::emitSnippetBody()
   {
   // doResolve:
   //    call resolveVirtualDispatchReadOnly

   uint8_t *cursor = cg()->getBinaryBufferCursor();

   getSnippetLabel()->setCodeLocation(cursor);

   TR::SymbolReference *resolveVirtualDispatchReadOnlySymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64resolveVirtualDispatchReadOnly, false, false, false);

   TR::GlobalFunctionCallData dataDestination(resolveVirtualDispatchReadOnlySymRef, _callNode, cursor, cg());
   cursor = cg()->getObjFmt()->encodeGlobalFunctionCall(dataDestination);

   uint8_t *helperCallRA = cursor;

   gcMap().registerStackMap(helperCallRA, cg());


   //   dd [RIP offset to vtableData]      ; 32-bit (position independent).  Relative to the start of the RA
   //                                      ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   TR_ASSERT_FATAL(IS_32BIT_RIP(_resolveVirtualDataAddress, helperCallRA), "resolve data is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(_resolveVirtualDataAddress - (intptr_t)helperCallRA);
   cursor += 4;

   //   dd [RIP offset to vtable index load in mainline]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                     ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   intptr_t loadResolvedVtableOffsetLabelAddress = reinterpret_cast<intptr_t>(_loadResolvedVtableOffsetLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(loadResolvedVtableOffsetLabelAddress, helperCallRA), "load resolved vtable label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(loadResolvedVtableOffsetLabelAddress - (intptr_t)helperCallRA);
   cursor += 4;

   //   dd [RIP offset to post vtable dispatch instruction]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                        ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   intptr_t doneLabelAddress = reinterpret_cast<intptr_t>(_doneLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(doneLabelAddress, helperCallRA), "done label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(doneLabelAddress - (intptr_t)helperCallRA);
   cursor += 4;

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ResolveVirtualDispatchReadOnlyDataSnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   // call resolveVirtualDispatchReadOnly
   //
   TR::SymbolReference *resolveVirtualDispatchReadOnlySymRef = _cg->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64resolveVirtualDispatchReadOnly, false, false, false);

   printPrefix(pOutFile, NULL, bufferPos, _cg->getObjFmt()->globalFunctionCallBinaryLength());
   trfprintf(pOutFile, "call\t[RIP+???] # %s, %s " POINTER_PRINTF_FORMAT,
                 getName(resolveVirtualDispatchReadOnlySymRef),
                 commentString(),
                 resolveVirtualDispatchReadOnlySymRef->getMethodAddress());
   bufferPos += _cg->getObjFmt()->globalFunctionCallBinaryLength();

   // dd [RIP offset to vtableData]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to vtableData",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   // dd [RIP offset to vtable index load in mainline]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to vtable index load in mainline",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   // dd [RIP offset to post vtable dispatch instruction]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to post vtable dispatch instruction",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());

   }

uint8_t *
TR::X86CallReadOnlySnippet::emitSnippetBody()
   {
   TR::Compilation*     comp         = cg()->comp();
   TR_J9VMBase*         fej9         = (TR_J9VMBase *)(cg()->fe());
   TR::SymbolReference* methodSymRef = _realMethodSymbolReference ? _realMethodSymbolReference : getNode()->getSymbolReference();
   TR::MethodSymbol*    methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   uint8_t*             cursor       = cg()->getBinaryBufferCursor();

   bool needToSetCodeLocation = true;
   bool isJitInduceOSRCall = false;

   if (comp->target().is64Bit() &&
       methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      isJitInduceOSRCall = true;
      }

   if (comp->target().is64Bit())
      {
      // Backspill register linkage arguments to the stack.
      //
      TR::Linkage *linkage = cg()->getLinkage(methodSymbol->getLinkageConvention());
      getSnippetLabel()->setCodeLocation(cursor);
      cursor = linkage->storeArguments(getNode(), cursor, false, NULL);
      needToSetCodeLocation = false;

      if (cg()->hasCodeCacheSwitched() &&
          (methodSymRef->getReferenceNumber()>=TR_AMD64numRuntimeHelpers))
         {
         fej9->reserveTrampolineIfNecessary(comp, methodSymRef, true);
         }
      }

   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
   if (comp->getOption(TR_UseSymbolValidationManager))
      forceUnresolvedDispatch = false;

   if (needToSetCodeLocation)
      {
      getSnippetLabel()->setCodeLocation(cursor);
      }

   // A value of 0 means no bypass is required
   //
   uint8_t *unresolvedBypassJumpDisplacement = 0;

   if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
      {

      J9::X86::AMD64::PrivateLinkage::ccUnresolvedStaticSpecialData *ccData = reinterpret_cast<J9::X86::AMD64::PrivateLinkage::ccUnresolvedStaticSpecialData *>(_unresolvedStaticSpecialDataAddress);
      ccData->snippetOrCompiledMethod = reinterpret_cast<intptr_t>(getSnippetLabel()->getCodeLocation());

      // Unresolved interpreted dispatch snippet shape:
      //
      // unresolved:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      //       test  rdi, rdi
      //       jne alreadyResolved
      //       call  [RIP+???]       ; interpreterUnresolved{Static|Special}GlueReadOnly
      //       dd    RIP offset to unresolved data area
      //       dd    cpIndex
      // interpreted:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      // alreadyResolved:
      //       jmp   [RIP+???]       ; interpreterStaticAndSpecialGlueReadOnly
      //
      TR_ASSERT_FATAL(!isJitInduceOSRCall || !forceUnresolvedDispatch, "calling jitInduceOSR is not supported yet under AOT\n");

      if (comp->getOption(TR_EnableHCR))
         {
         TR_ASSERT_FATAL(0, "HCR not supported for read only");
/*
         cg()->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(cursor);
*/
         }

      TR_ASSERT_FATAL((methodSymbol->isStatic() || methodSymbol->isSpecial() || forceUnresolvedDispatch), "Unexpected unresolved dispatch");

      // mov rdi, [RIP+???]     resolved RAM method
      //
      uint8_t *nextInstruction = cursor+7;

      // mov rdi, qword [RIP+???]
      //
      *cursor++ = 0x48;
      *cursor++ = 0x8b;
      *cursor++ = 0x3d;
      TR_ASSERT_FATAL(IS_32BIT_RIP(_ramMethodDataAddress, nextInstruction), "ram method data area is out of RIP-relative range");
      *(int32_t*)cursor = (int32_t)(_ramMethodDataAddress - (intptr_t)nextInstruction);
      cursor += 4;

      // test rdi, rdi
      //
      *cursor++ = 0x48;
      *cursor++ = 0x85;
      *cursor++ = 0xff;

      // jne alreadyResolved
      //
      *cursor++ = 0x75;
      unresolvedBypassJumpDisplacement = cursor++;   // leave a byte for the displacement

      // CALL interpreterUnresolved{Static|Special}GlueReadOnly
      //
      TR_RuntimeHelper resolutionHelper = methodSymbol->isStatic() ?
         TR_AMD64interpreterUnresolvedStaticGlueReadOnly : TR_AMD64interpreterUnresolvedSpecialGlueReadOnly;

      TR::SymbolReference *helperSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(resolutionHelper, false, false, false);

      TR::GlobalFunctionCallData data(helperSymRef, getNode(), cursor, cg());
      cursor = cg()->getObjFmt()->encodeGlobalFunctionCall(data);

      uint8_t *helperCallRA = cursor;

      // dd [RIP offset to unresolved static/special data area]
      //
      TR_ASSERT_FATAL(IS_32BIT_RIP(_unresolvedStaticSpecialDataAddress, helperCallRA), "unresolved data area is out of RIP-relative range");
      *(int32_t*)cursor = (int32_t)(_unresolvedStaticSpecialDataAddress - (intptr_t)helperCallRA);
      cursor += 4;

      // dd cpIndex
      //
      *(uint32_t *)cursor = methodSymRef->getCPIndexForVM();
      cursor += 4;
      }
   else
      {
      // Interpreted dispatch snippet shape
      //
      // interpreted:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      //       jmp   [RIP+???]       ; interpreterStaticAndSpecialGlueReadOnly
      //
      J9::X86::AMD64::PrivateLinkage::ccInterpretedStaticSpecialData *ccData = reinterpret_cast<J9::X86::AMD64::PrivateLinkage::ccInterpretedStaticSpecialData *>(_interpretedStaticSpecialDataAddress);
      ccData->snippetOrCompiledMethod = reinterpret_cast<intptr_t>(getSnippetLabel()->getCodeLocation());
      }

   if (!isJitInduceOSRCall)
      {
      uint8_t *nextInstruction = cursor+7;

      // mov rdi, qword [RIP+???]
      //
      *cursor++ = 0x48;
      *cursor++ = 0x8b;
      *cursor++ = 0x3d;
      TR_ASSERT_FATAL(IS_32BIT_RIP(_ramMethodDataAddress, nextInstruction), "ram method data area is out of RIP-relative range");
      *(int32_t*)cursor = (int32_t)(_ramMethodDataAddress - (intptr_t)nextInstruction);
      cursor += 4;
      }

   if (unresolvedBypassJumpDisplacement)
      {
      int32_t disp = (int32_t)(cursor - (unresolvedBypassJumpDisplacement+1));
      TR_ASSERT_FATAL(IS_8BIT_SIGNED(disp), "unresolved bypass branch too large");

      *unresolvedBypassJumpDisplacement = (uint8_t)disp;
      }

   // JMP interpreterStaticAndSpecialGlueReadOnly
   //
   TR::SymbolReference *helperSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64interpreterStaticAndSpecialGlueReadOnly, false, false, false);
   TR::GlobalFunctionCallData data(helperSymRef, getNode(), cursor, cg(), false /* JMP */);
   cursor = cg()->getObjFmt()->encodeGlobalFunctionCall(data);

   return cursor;
   }


uint32_t TR::X86CallReadOnlySnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint32_t length = 0;

   TR::SymbolReference *methodSymRef = _realMethodSymbolReference ? _realMethodSymbolReference : getNode()->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   if (cg()->comp()->target().is64Bit())
      {
      TR::Linkage *linkage = cg()->getLinkage(methodSymbol->getLinkageConvention());

      int32_t codeSize;
      (void)linkage->storeArguments(getNode(), NULL, true, &codeSize);
      length += codeSize;
      }

   TR::Compilation *comp = cg()->comp();
   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
   if (comp->getOption(TR_UseSymbolValidationManager))
      forceUnresolvedDispatch = false;

   if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
      {
      length += (   7    // mov rdi, [RAM method]
                  + 3    // test rdi, rdi
                  + 2    // jne already resolved
                  + cg()->getObjFmt()->globalFunctionCallBinaryLength()
                  + 4    // RIP to data area
                  + 4);  // cpIndex
      }

  bool isJitInduceOSRCall = false;
  if (comp->target().is64Bit() &&
       methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      isJitInduceOSRCall = true;
      }

   if (!isJitInduceOSRCall)
      {
      length += 7;  // mov rdi, [RIP+???] for RAM method
      }

   length += cg()->getObjFmt()->globalFunctionCallBinaryLength(); // JMP [RIP + ???]

   return length;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86CallReadOnlySnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   static const char callRegName64[][5] =
      {
      "rax","rsi","rdx","rcx"
      };

   const char callRegName32[][5] =
      {
      "eax","esi","edx","ecx"
      };

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::Node *callNode = snippet->getNode();
   TR::SymbolReference *methodSymRef = snippet->getRealMethodSymbolReference() ? snippet->getRealMethodSymbolReference() : callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

  if (_comp->target().is64Bit())
      {
      int32_t count;
      int32_t size = 0;
      int32_t offset = 8 * callNode->getNumChildren();

      for (count = 0; count < callNode->getNumChildren(); count++)
         {
         TR::Node *child = callNode->getChild(count);
         TR::DataType type = child->getType();

         switch (type)
            {
            case TR::Float:
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "mov \tdword ptr[rsp+%d], %s\t\t#save registers for interpreter call snippet",
                             offset, callRegName32[count] );
               offset-=8;
               bufferPos+=4;
               break;
            case TR::Double:
            case TR::Address:
            case TR::Int64:
               printPrefix(pOutFile, NULL, bufferPos, 5);
               trfprintf(pOutFile, "mov \tqword ptr[rsp+%d], %s\t\t#save registers for interpreter call snippet",
                             offset, callRegName64[count] );
               offset-=8;
               bufferPos+=5;
               break;
            default:
               TR_ASSERT_FATAL(0, "unknown data type: %d", type);
               break;
            }
         }
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());

   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
   if (_comp->getOption(TR_UseSymbolValidationManager))
      forceUnresolvedDispatch = false;

   if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
      {
      // Unresolved interpreted dispatch snippet shape:
      //
      // unresolved:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      //       test  rdi, rdi
      //       jne alreadyResolved
      //       call  [RIP+???]       ; interpreterUnresolved{Static|Special}GlueReadOnly
      //       dd    RIP offset to unresolved data area
      //       dd    cpIndex
      // interpreted:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      // alreadyResolved:
      //       jmp   [RIP+???]       ; interpreterStaticAndSpecialGlueReadOnly

      printPrefix(pOutFile, NULL, bufferPos, 7);
      trfprintf(pOutFile, "mov \trdi, [RIP+???] # load resolved RAM method ");
      bufferPos += 7;

      printPrefix(pOutFile, NULL, bufferPos, 3);
      trfprintf(pOutFile, "test \trdi, rdi");
      bufferPos += 3;

      printPrefix(pOutFile, NULL, bufferPos, 2);
      trfprintf(pOutFile, "jne \talreadyResolved");
      bufferPos += 2;

      printPrefix(pOutFile, NULL, bufferPos, _cg->getObjFmt()->globalFunctionCallBinaryLength());
      trfprintf(pOutFile, "call \t[RIP+???] # interpreterUnresolved{Static|Special}GlueReadOnly");
      bufferPos += _cg->getObjFmt()->globalFunctionCallBinaryLength();

      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "dd \tRIP offset to unresolved data area");
      bufferPos += 4;

      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "dd \tcpIndex");
      bufferPos += 4;
      }
   else
      {
      // Interpreted dispatch snippet shape
      //
      // interpreted:
      //       mov   rdi, [RIP+???]  ; resolved RAM Method
      //       jmp   [RIP+???]       ; interpreterStaticAndSpecialGlueReadOnly
      //
      }

   if (!(_comp->target().is64Bit() &&
       methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper()))
      {
      printPrefix(pOutFile, NULL, bufferPos, 7);
      trfprintf(pOutFile, "mov \trdi, [RIP+???] # load resolved RAM method ");
      bufferPos += 7;
      }

   printPrefix(pOutFile, NULL, bufferPos, _cg->getObjFmt()->globalFunctionCallBinaryLength());
   trfprintf(pOutFile, "jmp \t[RIP+???] # interpreterStaticAndSpecialGlueReadOnly");
   bufferPos += _cg->getObjFmt()->globalFunctionCallBinaryLength();

   }


uint32_t TR::X86InterfaceDispatchReadOnlySnippet::getLength(int32_t estimatedSnippetStart)
   {
   // call (6) + dd (4) + dd (4) + dd (4)
   //
   return 6 + 4 + 4 + 4;
   }


uint8_t *TR::X86InterfaceDispatchReadOnlySnippet::emitSnippetBody()
   {
   // slowInterfaceDispatch:
   //
   uint8_t *cursor = cg()->getBinaryBufferCursor();

   getSnippetLabel()->setCodeLocation(cursor);

   // call [RIP + slowInterfaceDispatchMethod]
   //
   intptr_t slowDispatchDataAddress = _interfaceDispatchDataAddress + offsetof(J9::X86::AMD64::PrivateLinkage::ccInterfaceData, slowInterfaceDispatchMethod);
   TR_ASSERT_FATAL(IS_32BIT_RIP(slowDispatchDataAddress, cursor+6), "slowDispatchDataAddress is out of RIP-relative range");

   *cursor++ = 0xff;  // CALLMem
   *cursor++ = 0x15;  // RIP+disp32
   *(int32_t*)cursor = (int32_t)(slowDispatchDataAddress - (reinterpret_cast<intptr_t>(cursor) + 4));
   cursor += 4;

   gcMap().registerStackMap(cursor, cg());

   //   dd [RIP offset to interfaceDispatchData]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                             ;    of the call to `slowInterfaceDisptchMethod`
   //
   TR_ASSERT_FATAL(IS_32BIT_RIP(_interfaceDispatchDataAddress, cursor), "interface dispatch data is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(_interfaceDispatchDataAddress - (intptr_t)cursor);
   cursor += 4;

   //   dd [RIP offset to first slot compare in mainline]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                      ;    of the call to `slowInterfaceDisptchMethod`
   //
   intptr_t slotRestartLabelAddress = reinterpret_cast<intptr_t>(_slotRestartLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(slotRestartLabelAddress, cursor-4), "slot restart label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(slotRestartLabelAddress - (intptr_t)cursor + 4);
   cursor += 4;

   //   dd [RIP offset to post interface dispatch instruction]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                           ;    of the call to `slowInterfaceDisptchMethod`
   //
   intptr_t doneLabelAddress = reinterpret_cast<intptr_t>(_doneLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(doneLabelAddress, cursor-8), "done label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(doneLabelAddress - (intptr_t)cursor + 8);
   cursor += 4;

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86InterfaceDispatchReadOnlySnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   // call [RIP + slowInterfaceDispatchMethod]
   //
   TR::SymbolReference *resolveVirtualDispatchReadOnlySymRef = _cg->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64resolveVirtualDispatchReadOnly, false, false, false);

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "call\t[rip + slowInterfaceDispatchSlot]  \t\t%s ", commentString());
   bufferPos += 6;

   //   dd [RIP offset to interfaceDispatchData]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                             ;    of the call to `slowInterfaceDisptchMethod`
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to interfaceDispatchData",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   //   dd [RIP offset to first slot compare in mainline]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                      ;    of the call to `slowInterfaceDisptchMethod`
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to first slot compare in mainling",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   //   dd [RIP offset to post interface dispatch instruction]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                           ;    of the call to `slowInterfaceDisptchMethod`
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to post interface dispatch instruction",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());

   }



